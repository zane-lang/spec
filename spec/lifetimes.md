# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: guest and borrow scope checks, moves, and deterministic destruction. It builds on the host, guest, and borrow storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for hosting and storage, §4 for references. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 Guest assignment uses host scope
A guest assignment is legal only when the target's host is declared in the same or a higher lexical scope than the guest itself.

```zane
outer Car(Node())
r &Node = outer.node
{
    innerCar Car(Node())
    r = innerCar.node // ILLEGAL: host's scope is nested relative to the guest
}
```

The compiler compares declaration scopes. It does not perform borrow inference or lifetime annotation solving.

This check works together with the source restriction in [`memory.md`](memory.md) §2.8: a guest may be created only from a field or another guest, never from a bare symbol. The source rule ensures the guest names storage that no move can empty and no container can relocate; the scope rule ensures that storage outlives the guest.

A **borrow** (`'T`) needs no such check. It cannot be stored or returned ([`memory.md`](memory.md) §2.9), so it never outlives the call that created it.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#inheriting-a-debt-safety-without-a-borrow-checker) — "Inheriting a debt: safety without a borrow checker".

### 1.2 Move-sources are bare symbols or hosting verb results
A move-source must denote a **hosting value the expression is entitled to consume**. Two forms qualify:

- a **bare symbol**: a local binding or parameter that hosts the object and is named directly by an identifier expression
- a **hosting verb result**: a value returned by a verb (function, method, operator, constructor, or lambda) whose return type is a hosting `T`. A hosting verb result has no source host; its source scope is the producing expression, which is always nested within or equal to the destination host's scope, so it satisfies the destination-scope restriction trivially.

A verb that returns a hosting `T` produces a fresh value that no symbol, field, or container hosts yet. Moving it transfers hosting of that temporary straight into the destination, so it re-parents nothing.

The following are **not** move-sources:
- a guest or a borrow (neither hosts, so neither can transfer hosting; see [`memory.md`](memory.md) §2.4 and §2.9)
- a field access such as `car.engine`
- a container element access such as `cars[1]`
- any other access path that projects into an existing host

```zane
engine Engine()
car Car(engine)             // legal: engine is a bare symbol
boat Boat(makeEngine())     // legal: makeEngine() returns a hosting Engine

truck Truck(car.engine)     // ILLEGAL: field access is not a move-source
truck2 Truck(makeCar().engine) // ILLEGAL: field access on temporary is not a move-source
garage Garage(cars[1])      // ILLEGAL: container element is not a move-source
```

This rule keeps containers stable hosting subtrees. Once a value is hosted by a field or stored in a container element, it cannot be individually moved out. The containing object may be moved as a whole if it is itself a move-source.

That a bare symbol is the *only* movable storage is what makes the guest source rule in [`memory.md`](memory.md) §2.8 sufficient: every other storage position can be overwritten but never emptied, so a guest rooted in one always finds a live occupant.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#what-may-be-moved-keeping-ownership-subtrees-whole) — "What may be moved: keeping ownership subtrees whole".

### 1.3 Moves are restricted to the declaration block
A bare symbol may only be used as a move-source in the exact lexical block where that symbol was declared. Host parameters may be used as move-sources at the top level of the function body. A parameter is not part of the body scope, though: it belongs to the **call-site scope** (§1.5).

```zane
engine Engine()
car Car(engine)          // legal: same block as engine's declaration

{
    node Node()
    innerOwner Node = node // legal: same block as node's declaration
}
```

Moving an outer symbol from a nested block is illegal:

```zane
car Car()
{
    garage Garage(car)   // ILLEGAL: car was declared in outer block
}
```

This restriction prevents conditional moves and flow-dependent host changes. If control flow is needed, compute the destination or guard condition first, then perform a single move in the symbol's declaration block.

Confining moves this way is also what makes an object's resting scope statically known, which is what lets it be allocated once and never relocated ([`memory.md`](memory.md) §3.5).

The restriction applies only to symbol move-sources. A hosting verb result (§1.2) is an unnamed temporary with no declaration block, so it is simply consumed at the point where it appears.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-declaration-block-rule-and-the-flow-analysis-it-refuses) — "The declaration-block rule, and the flow analysis it refuses".

### 1.4 Destination scope must contain or match source scope
A value may move into a new host only when the destination host is declared in the same or a higher lexical scope than the source host.

```zane
node Node()
{
    nestedOwner Node()
    nestedOwner = node // ILLEGAL: cannot move into a host declared in a nested scope
}
```

A hosting verb result (§1.2) has no source host; its source scope is the expression that produces it. That scope is always nested within or equal to the destination host's scope, so this restriction is trivially satisfied and never blocks moving a verb result into any host.

A parameter's value is exempt. Because a parameter belongs to the call-site scope and is not part of the body (§1.5), lending it into a local or a nested call does not sink hosting into that lower scope.

This rule has a second job beyond safety. Because every destination is at the same or a higher scope, the candidate destinations for a given object all lie on the ancestor chain of its declaration scope — a chain totally ordered by nesting, which therefore has an outermost member. That member bounds where the object may come to rest, which is what lets the compiler materialize it once, in that scope's arena ([`memory.md`](memory.md) §3.5), so it never needs to be relocated in order to outlive the scope it was written in.

### 1.5 Parameters belong to the call site
A reference-type parameter is **not part of the callee's body scope**. It behaves as a symbol in the **call-site scope**, one level above the body. Passing a hosting reference-type value to a swallowing `T` parameter lends it in with hosting access, but the value's lifetime stays with the call site.

This is what makes the passing rule safe. Because the parameter is not part of the body scope, the body draining never destroys the value. The body may read it, move it into a local, or pass it to a nested call; when a local that received it exits, the value is not dropped — the compiler moves hosting back up to the call site, and the chain repeats outward until the scope that first hosted the value drains.

```zane
Unit enterMatch(player Player) {
    island Island = makeIsland()
    island!startMatch(player) // player is lent into the local island
    return Unit()
}
```

`startMatch` puts `player` into the local `island`. Because `player` belongs to the call site, `island` draining does not destroy it. Inside `enterMatch`, `player` was passed by hosting access, so `enterMatch`'s `player` symbol has downgraded (§1.8) — and so has the argument symbol in whatever called `enterMatch`.

For `&` fields specifically, the callee must declare the corresponding parameter as `&T` ([`memory.md`](memory.md) §2.9). Binding a swallowed parameter or a borrow into `&` storage is a compile-time error.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#consumed-or-borrowed-the-parameter-that-lives-at-the-call-site) — "Consumed or borrowed: the parameter that lives at the call site".

### 1.6 Moved symbols downgrade and are no longer movable
After a bare symbol is moved, that symbol is **downgraded**: it loses hosting but keeps denoting the same object. It remains readable and cannot be moved again.

```zane
engine Engine()
car Car(engine)          // engine is moved; downgrades
engine:inspect()         // legal: engine still denotes the same object
truck Truck(engine)      // ILLEGAL: engine no longer hosts, so it is not a move-source
```

The mechanism is that a move relocates nothing ([`memory.md`](memory.md) §3.7). A bare symbol is a *name bound to* an object rather than storage containing it ([`memory.md`](memory.md) §3.5), so a move re-attaches hosting to the destination while the downgraded symbol goes on naming the same object. Reads through it therefore reach what they always did. What the source loses is only the right to move again.

A downgraded symbol is still not a guest source ([`memory.md`](memory.md) §2.8): the restriction is on bare symbols as a storage form, not on whether one currently hosts.

This also applies across calls. Passing a hosting value to a swallowing `T` parameter downgrades the caller's symbol (§1.8); the caller can still read the symbol afterward. Zane has no user-visible use-after-move error class for reads.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#downgrade-not-poison-why-there-is-no-use-after-move-read) — "Downgrade, not poison: why there is no use-after-move-read".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#moving-without-relocating) — "Moving without relocating".

A hosting verb result (§1.2) has no symbol to downgrade. The temporary is consumed by the move and cannot be named again, so the double-move question never arises for it.

### 1.7 Returned guests must be rooted in a guest parameter
A function may return an `&T` only when the returned reference is rooted in one of the function's **guest** parameters. `this &T` counts as a guest parameter.

```zane
&Weapon getWeapon(this &Player) => this.weapon
```

The receiver is written `this &Player` rather than `this Player`, because a plain reference-type receiver is a borrow ([`memory.md`](memory.md) §2.9) and a borrow may not be returned. Declaring it a guest receiver moves the obligation to the call site, which must then supply a guest source:

```zane
w &Weapon = world.player:getWeapon()   // legal: world.player is a field
w2 &Weapon = loosePlayer:getWeapon()   // ILLEGAL: bare symbol is not a guest source
```

A borrow parameter may never be returned, and neither may anything rooted in one:

```zane
&Node bad(node 'Node) {
    return node    // ILLEGAL: a borrow cannot escape the call
}
```

```zane
&Node alsoBad() {
    value Node()
    return value   // ILLEGAL: returned guest is not rooted in a guest parameter
}
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#returning-a-ref-without-a-lifetime-to-name-it) — "Returning a ref without a lifetime to name it".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#moving-without-relocating) — "Moving without relocating".

### 1.8 The signature decides what the caller gives up
A reference-type parameter is written in one of three modes ([`memory.md`](memory.md) §2.9), and the mode is the whole contract. Nothing in the callee's body changes the outcome the signature already states, so there is no interprocedural inference: whether a passed host downgrades never depends on the callee's body or on the build.

- **Swallow (`T`)** takes the argument by hosting access, using it as a move-source (§1.2), so the caller's symbol downgrades (§1.6) — **whatever the callee does with the value**.
- **Guest (`&T`)** requires the caller to supply a guest source ([`memory.md`](memory.md) §2.8). The caller keeps hosting, and the callee may store or return the guest.
- **Borrow (`'T`)** takes non-hosting, non-escaping access for the call. The caller keeps hosting, may pass anything it has including a bare symbol, and the callee may neither store nor return it.

```zane
car Car()
garage!store(car)     // store takes `Car`: car downgrades
car:inspect()         // legal: car still denotes the same object
truck Truck(car)      // ILLEGAL: car no longer hosts
```

The value outlives the call (§1.5), so a downgraded symbol always denotes a live object.

A verb that only reads its reference argument should declare it `'T`. Declaring it plain `T` is still legal — reading does not change the fact that the signature asked for hosting access, so the caller downgrades all the same. Leaving a parameter entirely unused is a separate, general matter: a release build rejects an unused parameter whether it hosts a value or not.

To keep or recover hosting, either pass `'T`, pass `&T`, or bind a relayed return:

```zane
weapon Weapon()
weapon2 Weapon = reforge(weapon)   // reforge relays the host; weapon2 hosts the result
```

Here `startMatch` consumes `player` into `island`, so `player` downgrades; `enterMatch` then recovers hosting from `returnPlayer`'s return. Reassigning `player` overwrites its hosting slot ([`memory.md`](memory.md) §2.2), so the moved-from symbol hosts again and `return player` is an ordinary move:

```zane
Player enterMatch(player Player) {
    island Island = makeIsland()
    playerId Int = player.id
    island!startMatch(player)              // startMatch consumes player; player downgrades
    player = island!returnPlayer(playerId) // recover hosting; player is a full host again
    return player
}

Unit main() {
    player Player = makePlayer()
    player = enterMatch(player)            // bind to regain hosting privilege; unbound, the host floats (§1.9)
    return Unit()
}
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".

### 1.9 An ignored hosting result floats to the enclosing scope
A return value need not be bound. When a call's result is a reference-type host and the call stands as a bare statement, that host is not destroyed at the end of the statement — it **floats**: it becomes an anonymous host in the enclosing scope and lives until that scope drains, like any object hosted by that scope (§2.1). An ignored value-type result, including `Unit()`, is simply discarded.

Binding the return is how the caller takes **hosting privilege**. A bound host may be moved again; a floated one may not.

```zane
car2 Car = repair(car)   // bind: car2 is a full host, and may be moved again
repair(car)              // legal: the returned host floats to the enclosing scope
```

Because a floated result is kept rather than dropped, no guest dangles and no hosted object is silently destroyed. What binding controls is not safety but privilege: whether the result returns as a movable host. A value-type result has no host; ignoring one simply discards the value.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".

---

## 2. Lifetime and Destruction

### 2.1 Destruction is deterministic
A reference-type instance is destroyed when its host is overwritten, when its hosting container dies, or when its hosting scope drains under the concurrency rules. Destruction runs at that point; the memory it occupies is reclaimed when the arena is unmapped ([`memory.md`](memory.md) §3.2).

### 2.2 Scopes drain before destruction
If a scope launches concurrent work, objects hosted by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule (see [`concurrency.md`](concurrency.md) §4.1).

### 2.3 Guest storage never extends lifetime
Guests do not participate in hosting and cannot prolong object lifetime. They only reach a live object whose host is already guaranteed to outlive them. A borrow likewise extends nothing; it merely cannot outlive the call.

### 2.4 Null guests are not a user-facing state
Because every storage position is directly initialized ([`memory.md`](memory.md) §2.11), a host slot always holds a live occupant, and the guest source and scope rules prevent a guest from outliving its target, the runtime does not expose a null-guest programming model to the user.

---

## 3. Language Comparisons

### 3.1 Lifetime and destruction behavior

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Destruction timing | deterministic | non-deterministic | deterministic | manual / RAII |
| GC pauses | ❌ | ✅ | ❌ | ❌ |
| Dangling guest risk | ❌ | ❌ | ❌ | ✅ |
| Lifetime annotations | ❌ | ❌ | ✅ | ❌ |

---

## 4. Summary

| Concept | Rule |
|---|---|
| Guest assignment | Only from a field or another guest — never a bare symbol, a `[]`, or a path rooted in one — and only when the target's host is in the same or a higher lexical scope than the guest |
| Borrow | Needs no scope check; it cannot be stored or returned, so it cannot outlive its call |
| Guest return | A returned `&T` must be rooted in a guest parameter; `this &T` counts. A borrow may never be returned |
| Move-source | A bare symbol (local or parameter) or a hosting verb result; not a guest, borrow, field, container element, or other access path |
| Move declaration-block restriction | A bare symbol may only be moved in the exact lexical block where it was declared; parameters may be moved at the body top level |
| Move destination scope | Destination host must be in the same or a higher lexical scope than the source host; this is what makes the object's resting scope static |
| Move mechanism | Transfers hosting at compile time; the object is never relocated and no payload bytes are copied |
| Post-move downgrade | After a move, the source symbol loses hosting but keeps denoting the same object; it remains readable and is no longer a move-source |
| Parameter scope | A reference parameter belongs to the call-site scope, not the body, so a value passed by hosting access outlives the call |
| Passing mode | `T` swallows and downgrades the caller; `&T` is a guest the caller lends while remaining host; `'T` is a borrow that ends with the call |
| Return value | A return need not be bound; an unbound reference-type result floats to the enclosing scope as an anonymous host |
| Destruction | Deterministic: at host overwrite, container death, or scope drain |

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#no-rule-to-spare-the-specific-hole-each-restriction-plugs) — "No rule to spare: the specific hole each restriction plugs".
