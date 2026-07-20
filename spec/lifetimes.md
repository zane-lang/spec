# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: `&` assignment scope checks, rehosting, and deterministic destruction. It builds on the host and guest storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for hosting and storage, §4 for anchors and tethers. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 `&` assignment uses host scope
An `&` assignment is legal only when the target's host is declared in the same or a higher lexical scope than the `&` itself.

```zane
outer Node()
r &Node = outer
{
    innerNode Node()
    r = innerNode // ILLEGAL: host's scope is nested relative to the guest
}
```

The compiler compares declaration scopes. It does not perform borrow inference or lifetime annotation solving.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#inheriting-a-debt-safety-without-a-borrow-checker) — "Inheriting a debt: safety without a borrow checker".

### 1.2 Move-sources are host symbols or hosting verb results
A move-source must denote an **hosted value the expression is entitled to consume**. Two forms qualify:

- a **direct host symbol**: a local binding or parameter that hosts the object and is named directly by an identifier expression
- an **hosted verb result**: a value returned by a verb (function, method, operator, constructor, or lambda) whose return type is a hosted `T`

A verb that returns a hosting `T` produces a fresh value that no symbol, field, or container hosts yet. Moving it transfers hosting of that temporary straight into the destination, so it re-parents nothing.

The following are **not** move-sources:
- an `&` value, including a verb that returns `&T` (guests are non-hosting and cannot transfer hosting; see [`memory.md`](memory.md) §2.4)
- a field access such as `car.engine`
- a container element access such as `cars[1]`
- any other access path that projects into an existing host

```zane
engine Engine()
car Car(engine)             // legal: engine is a direct host symbol
boat Boat(makeEngine())     // legal: makeEngine() returns a hosting Engine

truck Truck(car.engine)     // ILLEGAL: field access is not a move-source
truck2 Truck(makeCar().engine) // ILLEGAL: field access on temporary is not a move-source
garage Garage(cars[1])      // ILLEGAL: container element is not a move-source
```

This rule keeps containers stable hosted subtrees. Once a value is hosted by a field or stored in a container element, it cannot be individually moved out. The containing object may be moved as a whole if it is itself a move-source. A hosting verb result is exempt from the access-path restriction because it has no host until the move binds it.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#what-may-be-moved-keeping-ownership-subtrees-whole) — "What may be moved: keeping ownership subtrees whole".

### 1.3 Moves are restricted to the declaration block
A direct host symbol may only be used as a move-source in the exact lexical block where that symbol was declared. Host parameters may be used as move-sources at the top level of the function body. A parameter is not part of the body scope, though: it belongs to the **call-site scope** (§1.5). The caller that supplied a hosting argument has already downgraded to a guest (§1.8); moving the parameter within the body only decides where the value comes to rest.

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

```zane
Void loadCar(this Boat, car Car) mut {
    this.cars!append(car) // legal: car is moved into this.cars at the top level of the body
}
```

This restriction prevents conditional moves and flow-dependent host changes. If control flow is needed, compute the destination or guard condition first, then perform a single move in the symbol's declaration block.

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

A parameter's value is exempt. Because a parameter belongs to the call-site scope and is not part of the body (§1.5), lending it into a local or a nested call does not sink hosting into that lower scope: the value returns to the call site when the local exits, unless the callee moves it into another parameter's hosting storage or into the return (§1.8).

### 1.5 Parameters belong to the call site
A reference-type parameter is **not part of the callee's body scope**. It behaves as a symbol in the **call-site scope**, one level above the body. Passing a hosted reference-type value to a plain `T` parameter lends it in with hosting access, but the value's lifetime stays with the call site.

This is what makes the passing rule safe. Because the parameter is not part of the body scope, the body draining never destroys the value. The body may read it, move it into a local, or pass it to a nested call; when a local that received it exits, the value is not dropped — the compiler moves it back up to the call site, and the chain repeats outward until the scope that first hosted the value drains. A value passed by hosting access therefore always outlives the call, which is what lets the caller's symbol downgrade to a live guest (§1.8) rather than a dangling one.

```zane
Void enterMatch(player Player) {
    island Island = makeIsland()
    island!startMatch(player) // player is lent into the local island
}
```

`startMatch` puts `player` into the local `island`. Because `player` belongs to the call site, `island` draining does not destroy it; the value lives until `enterMatch`'s own scope drains. Inside `enterMatch`, `player` was passed to `startMatch` by hosting access, so `enterMatch`'s `player` symbol is now a guest to it (§1.8) — and so is the argument symbol in whatever called `enterMatch`.

For `&` fields specifically, the callee must declare the corresponding parameter as `&T` ([`memory.md`](memory.md) §2.9). Attempting to bind a plain `T` parameter into `&` storage is a compile-time error, because a swallowed value is hosted at the call site while an `&` field lives with the object that holds it, which may outlive the call. The callee's signature therefore signals whether an `&`-creating source ([`memory.md`](memory.md) §2.8) is required at the call site.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#consumed-or-borrowed-the-parameter-that-lives-at-the-call-site) — "Consumed or borrowed: the parameter that lives at the call site".

### 1.6 Moved symbols downgrade to `&` values and are no longer movable
After a direct host symbol is moved, that symbol is downgraded to an `&` value through the anchor (see [`memory.md`](memory.md) §4.5). The symbol remains readable but cannot be moved again.

```zane
engine Engine()
car Car(engine)          // engine is moved; downgrades to `&`
engine:inspect()         // legal: engine is now an `&`, still readable
truck Truck(engine)      // ILLEGAL: engine is an `&`, not a move-source
```

This also applies across calls. Passing a hosting value to a plain `T` parameter downgrades the caller's symbol to an `&` (§1.8); the caller can still read the symbol afterward through that downgraded `&`. Zane has no user-visible use-after-move error class for reads.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#downgrade-not-poison-why-there-is-no-use-after-move-read) — "Downgrade, not poison: why there is no use-after-move-read".

A hosting verb result (§1.2) has no symbol to downgrade. The temporary is consumed by the move and cannot be named again, so the double-move question never arises for it.

### 1.7 Returned `&` values must be rooted in a parameter
A function may return an `&T` only when the returned reference is rooted in one of the function's parameters. `this` counts as a parameter for this rule.

```zane
&Weapon getWeapon(this &Player) => this.weapon
```

```zane
&Int bad() {
    value Int = 1
    return value   // ILLEGAL: returned `&` is not rooted in a parameter
}
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#returning-a-ref-without-a-lifetime-to-name-it) — "Returning a ref without a lifetime to name it".

### 1.8 Passing a host to a `T` parameter downgrades it to a guest
A plain reference-type parameter `T` takes its argument by **hosting access**. Passing a hosting value to such a parameter uses that value as a move-source (§1.2), so the caller's symbol downgrades to a guest (§1.6) — **whatever the callee does with the value**. The parameter's declared type is the whole contract: `T` means the caller gives up hosting; `&T` (a guest, [`memory.md`](memory.md) §2.9) means the caller lends the value and stays a full host. Nothing in the callee's body changes the outcome the signature already states.

```zane
car Car()
garage!store(car)     // store takes `Car`: car downgrades to a guest
car:inspect()         // legal: car is still readable through the guest
truck Truck(car)      // ILLEGAL: car is a guest, not a move-source
```

The value outlives the call (§1.5), so the downgraded guest always resolves to a live object. Where the value comes to rest — moved into another parameter's hosting storage, moved into the return, or held in the call-site scope — the guest follows through the anchor ([`memory.md`](memory.md) §4.5).

A verb treats a reference-type host argument in one of three ways, each fixed by its signature:

- it takes a **guest** — declares the parameter `&T` ([`memory.md`](memory.md) §2.9); the caller stays a full host and lends only a guest for the call.
- it **relays** the host — declares a swallowing `T` and returns a hosting handle; the caller downgrades to a guest but may bind the return to host the object again (§1.9).
- it **consumes** the host — declares a swallowing `T` and returns no host; the caller downgrades to a guest, and the value stays wherever the verb placed it.

Passing a guest leaves the caller as host; relaying and consuming both downgrade it, differing only in whether a hosting handle is handed back. So to keep or recover hosting, either pass `&T` or bind a relayed return:

```zane
weapon Weapon()
weapon2 Weapon = reforge(weapon)   // reforge relays the host; weapon2 hosts the result
```

A relay that swallows a value and hands it back uses the return path. Here `startMatch` consumes `player` into `island`, so `player` downgrades to a guest; `enterMatch` then recovers hosting from `returnPlayer`'s return. Reassigning `player` overwrites its hosting slot ([`memory.md`](memory.md) §2.2), so the moved-from symbol is a host again and `return player` is an ordinary move:

```zane
Player enterMatch(player Player) {
    island Island = makeIsland()
    playerId Int = player.id
    island!startMatch(player)              // startMatch consumes player; player is now a guest
    player = island!returnPlayer(playerId) // recover hosting; player is a full host again
    return player
}

Void main() {
    player Player = makePlayer()
    player = enterMatch(player)            // bind to regain hosting privilege; unbound, the host floats (§1.9)
}
```

A verb that only reads its reference argument may still declare it plain `T`: reading does not change the fact that the signature asked for hosting access, so the caller downgrades all the same. Declaring the parameter `&T` is what keeps the caller as host. Because the signature alone decides the caller's state, there is no interprocedural consumption inference: whether a passed host downgrades never depends on the callee's body or on the build. Using hosting access only to read a value is legal. Leaving a parameter entirely unused is a separate, general matter — a release build rejects an unused parameter whether it hosts a value or not.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".

### 1.9 An ignored hosted result floats to the enclosing scope
A non-`Void` return need not be bound. When a call's result is a reference-type host and the call stands as a bare statement, that host is not destroyed at the end of the statement — it **floats**: it becomes an anonymous host in the enclosing scope and lives until that scope drains, like any scope-hosted value (§2.1).

Binding the return is how the caller takes **hosting privilege**. A bound host may be moved again; a floated one may not — the caller reaches it only through whatever guest it already holds (§1.8).

```zane
car2 Car = repair(car)   // bind: car2 is a full host, and may be moved again
repair(car)              // legal: the returned host floats to the enclosing scope
```

Because a floated result is kept rather than dropped, no guest dangles and no hosted value is silently destroyed. What binding controls is not safety but privilege: whether the result returns as a movable host or is merely reachable through a guest. This makes the caller's intent visible — a bound return is the signal that the caller wanted hosting back. A value-type result has no host or guest; ignoring one simply discards the value.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".

---

## 2. Lifetime and Destruction

### 2.1 Destruction is deterministic
Class instances are destroyed when their host dies, their hosting container dies, or their hosting scope drains under the concurrency rules.

### 2.2 Scopes drain before destruction
If a scope launches concurrent work, objects hosted by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule (see [`concurrency.md`](concurrency.md) §4.1).

### 2.3 Guest storage never extends lifetime
Guests do not participate in hosting and cannot prolong object lifetime. They only track a live object whose host is already guaranteed to outlive them.

### 2.4 Null guests are not a user-facing state
Because scope rules (§1.1) prevent guests from outliving their hosts, the runtime does not expose a normal “null guest” programming model to the user.

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
| `&` return | Returned `&T` must be rooted in a parameter; `this` counts |
| Guest assignment | Only from a place expression whose host is in the same or a higher lexical scope than the guest |
| Move-source | A direct host symbol (local or parameter) or a hosting verb result; not an `&`, field, container element, or other access path |
| Move declaration-block restriction | A direct host symbol may only be moved in the exact lexical block where it was declared; parameters may be moved at the body top level |
| Move destination scope | Destination host must be in the same or a higher lexical scope than the source host |
| Post-move downgrade | After a move, the source symbol downgrades to an `&` and remains readable but is no longer a move-source |
| Parameter scope | A reference parameter belongs to the call-site scope, not the body, so a value passed by hosting access outlives the call |
| Hosting argument | A verb takes a **guest** (`&T`, caller keeps it), **relays** the host (`T` and returns a hosting handle, caller may bind it to host again), or **consumes** it (`T`, no host returned, caller keeps a guest); passing to a plain `T` downgrades the caller to a guest whatever the body does |
| Return value | A non-`Void` return need not be bound; an unbound reference-type result floats to the enclosing scope as an anonymous host, and the caller keeps only a guest to it |
| Destruction | Deterministic and delayed until the hosting scope drains |
