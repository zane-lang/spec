# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: `&` assignment scope checks, rehosting, and deterministic destruction. It builds on the host and guest storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for hosting and storage, §4 for anchors and tethers. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 `&` assignment uses host scope
An `&` assignment is legal only when the source is a guest source ([`memory.md`](memory.md) §2.8) **and** the target's host is declared in the same or a higher lexical scope than the `&` itself.

```zane
outerTree Tree()
r &Node = outerTree.root
{
    innerTree Tree()
    r = innerTree.root // ILLEGAL: the host's scope is nested relative to the guest
}
```

The two conditions are independent. Nearly every place expression is a guest source ([`memory.md`](memory.md) §2.8), so in practice this rule is the scope comparison, and it is the comparison that does the work:

```zane
node Node()
r &Node = node   // legal: same scope
```

The compiler compares declaration scopes. It does not perform borrow inference or lifetime annotation solving.

An assignment into an `&` **field or element** carries one further condition, stated in §1.10: both paths must share a root symbol. The scope comparison above settles an `&` **symbol**, whose scope is fixed at its declaration; a field's scope is its container's, which a move can change, so it needs the additional rule.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#inheriting-a-debt-safety-without-a-borrow-checker) — "Inheriting a debt: safety without a borrow checker".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-root-rule-that-got-shorter) — "The root rule that got shorter".

### 1.2 Move-sources are host symbols, hosting verb results, or `#variant` case forms
A move-source must denote a **hosting value the expression is entitled to consume**. Three forms qualify:

- a **direct host symbol**: a local binding or parameter that hosts the object and is named directly by an identifier expression
- a **hosting verb result**: a value returned by a verb (function, method, operator, constructor, or lambda) whose return type is a hosting `T`. A hosting verb result has no source host; its source scope is the producing expression, which is always nested within or equal to the destination host's scope, so it satisfies the destination-scope restriction trivially.
- a **`#variant` case form**: `Variant.case(payload)` where `Variant` is a **reference** sum type (see [`adt.md`](adt.md) §3.2). It is built-in syntax rather than a verb, but it stands in the same position as a hosting verb result — it produces a fresh value nothing hosts yet — and it is a move-source on the same terms. A *value* `variant` case form is not one, and does not need to be: a value sum is copied rather than hosted, so there is no hosting to transfer. That holds even when the value owns boxed members, because the copy that reaches its destination is deep (see [`memory.md`](memory.md) §2.3).

A verb that returns a hosting `T`, and a case form that builds a `#variant`, both produce a fresh value that no symbol, field, or container hosts yet. Moving it transfers hosting of that temporary straight into the destination, so it re-parents nothing. This is what lets a recursive structure be written as one nested expression: each boxed hosting member takes the node built for it in place (see [`adt.md`](adt.md) §4).

The following are **not** move-sources:
- an `&` value, including a verb that returns `&T` (guests are non-hosting and cannot transfer hosting; see [`memory.md`](memory.md) §2.4)
- a value-type parameter or a value-type `mut` subject, both of which are borrows of the caller's slot (see [`memory.md`](memory.md) §2.9)
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

This rule keeps containers stable hosting subtrees. Once a value is hosted by a field or stored in a container element, it cannot be individually moved out. The containing object may be moved as a whole if it is itself a move-source. A hosting verb result and a `#variant` case form are exempt from the access-path restriction because neither has a host until the move binds it.

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
Unit loadCar(this Boat, car Car) mut {
    this.cars!append(car) // legal: car is moved into this.cars at the top level of the body
    return Unit()
}
```

This restriction prevents conditional moves and flow-dependent host changes. If control flow is needed, compute the destination or guard condition first, then perform a single move in the symbol's declaration block.

The restriction applies only to symbol move-sources. A hosting verb result or `#variant` case form (§1.2) is an unnamed temporary with no declaration block, so it is simply consumed at the point where it appears.

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

This rule constrains the moved value's own scopes. When the value **carries guests**, raising it also re-checks what those guests name; that is §1.11.

### 1.5 Parameters belong to the call site
A reference-type parameter is **not part of the callee's body scope**. It behaves as a symbol in the **call-site scope**, one level above the body. Passing a hosting reference-type value to a plain `T` parameter lends it in with hosting access, but the value's lifetime stays with the call site.

This is stated for the swallowing mode because that is the only mode where hosting crosses the call boundary at all. An `&T` guest parameter never takes hosting ([`memory.md`](memory.md) §2.9), so nothing about the argument's lifetime changes when one is used; the call-site scope keeps hosting throughout.

This is what makes the passing rule safe. Because the parameter is not part of the body scope, the body draining never destroys the value. The body may read it, move it into a local, or pass it to a nested call; when a local that received it exits, the value is not dropped — the compiler moves it back up to the call site, and the chain repeats outward until the scope that first hosted the value drains. A value passed by hosting access therefore always outlives the call, which is what lets the caller's symbol downgrade to a live guest (§1.8) rather than a dangling one.

```zane
Unit enterMatch(player Player) {
    island Island = makeIsland()
    island!startMatch(player) // player is lent into the local island
    return Unit()
}
```

`startMatch` puts `player` into the local `island`. Because `player` belongs to the call site, `island` draining does not destroy it; the value lives until `enterMatch`'s own scope drains. Inside `enterMatch`, `player` was passed to `startMatch` by hosting access, so `enterMatch`'s `player` symbol is now a guest to it (§1.8) — and so is the argument symbol in whatever called `enterMatch`.

For `&` fields specifically, the callee must declare the corresponding parameter as `&T` ([`memory.md`](memory.md) §2.9). Binding a plain `T` parameter into `&` storage is a compile-time error, because a swallowed value is hosted at the call site while an `&` field lives with the object that holds it, which may outlive the call. The callee's signature therefore signals which mode applies, and so what the caller gives up.

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
A function may return an `&T` only when the returned guest is rooted in one of the function's **parameters** — the parameter used bare, or a field access whose base chain reaches it. `this` counts as a parameter for this rule.

```zane
&Weapon getWeapon(this Player) => this.weapon
```

Both parameter modes are roots, and for the same reason: a parameter belongs to the **call-site scope** (§1.5), never to the body. A guest rooted in one therefore names something hosted in the scope the return value lands in, so §1.1 compares the two directly at the call site and rejects the cases that would dangle. A swallowing `T` parameter qualifies on exactly these terms — the value it took outlives the call (§1.5) — even though passing to it downgrades the caller (§1.8).

A **local** is the case this rule excludes, and it is excluded by lifetime rather than by what may mint a guest:

```zane
&Node bad() {
    value Node()
    return value   // ILLEGAL: value is hosted by the body scope, which drains at the return
}
```

This rule governs a return that **is** an `&T`. A return that *carries* one — a hosting value with an `&` reachable inside it — is raised into the call-site scope and governed by §1.11, on the same reasoning.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#returning-a-ref-without-a-lifetime-to-name-it) — "Returning a ref without a lifetime to name it".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-root-rule-that-got-shorter) — "The root rule that got shorter".

### 1.8 Passing a host to a `T` parameter downgrades it to a guest
A plain reference-type parameter `T` takes its argument by **hosting access**. Passing a hosting value to such a parameter uses that value as a move-source (§1.2), so the caller's symbol downgrades to a guest (§1.6) — **whatever the callee does with the value**. The parameter's declared type is the whole contract: `T` means the caller gives up hosting; `&T` ([`memory.md`](memory.md) §2.9) means the caller stays a full host. Nothing in the callee's body changes the outcome the signature already states.

```zane
car Car()
garage!store(car)     // store takes `Car`: car downgrades to a guest
car:inspect()         // legal: car is still readable through the guest
truck Truck(car)      // ILLEGAL: car is a guest, not a move-source
```

The value outlives the call (§1.5), so the downgraded guest always resolves to a live object. Where the value comes to rest — moved into another parameter's hosting storage, moved into the return, or held in the call-site scope — the guest follows through the anchor ([`memory.md`](memory.md) §4.5).

A verb treats a reference-type host argument in one of three ways, each fixed by its signature:

- it takes a **guest** — declares the parameter `&T`; the caller stays a full host, and the callee may read it, mutate it, or return it. It may not store the guest in an object it reaches through a different root (§1.10).
- it **relays** the host — declares a swallowing `T` and returns a hosting handle; the caller downgrades to a guest but may bind the return to host the object again (§1.9).
- it **consumes** the host — declares a swallowing `T` and returns no host; the caller downgrades to a guest, and the value stays wherever the verb placed it.

Taking a guest leaves the caller as host; relaying and consuming both downgrade it, differing only in whether a hosting handle is handed back. So to keep or recover hosting, pass `&T` or bind a relayed return:

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

Unit main() {
    player Player = makePlayer()
    player = enterMatch(player)            // bind to regain hosting privilege; unbound, the host floats (§1.9)
    return Unit()
}
```

A verb that only reads its reference argument may still declare it plain `T`: reading does not change the fact that the signature asked for hosting access, so the caller downgrades all the same. Declaring the parameter `&T` is what keeps the caller as host. Because the signature alone decides the caller's state, there is no interprocedural consumption inference: whether a passed host downgrades never depends on the callee's body or on the build. Using hosting access only to read a value is legal. Leaving a parameter entirely unused is a separate, general matter — a release build rejects an unused parameter whether it hosts a value or not.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".
> **Story:** [`stories/memory.md`](../stories/memory.md#three-ways-to-hand-over-an-object) — "Three ways to hand over an object".

### 1.9 An ignored hosting result floats to the enclosing scope
A return value need not be bound. When a call's result is a reference-type host and the call stands as a bare statement, that host is not destroyed at the end of the statement — it **floats**: it becomes an anonymous host in the enclosing scope and lives until that scope drains, like any object hosted by that scope (§2.1). An ignored value-type result, including `Unit()`, is simply discarded.

Binding the return is how the caller takes **hosting privilege**. A bound host may be moved again; a floated one may not — the caller reaches it only through whatever guest it already holds (§1.8).

```zane
car2 Car = repair(car)   // bind: car2 is a full host, and may be moved again
repair(car)              // legal: the returned host floats to the enclosing scope
```

Because a floated result is kept rather than dropped, no guest dangles and no hosted object is silently destroyed. What binding controls is not safety but privilege: whether the result returns as a movable host or is merely reachable through a guest. This makes the caller's intent visible — a bound return is the signal that the caller wanted hosting back. A value-type result has no host or guest; ignoring one simply discards the value.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".

### 1.10 An `&` field or element is written only from a path sharing its root
An assignment whose destination is an `&` **field or element** is legal only when the destination path and the source path begin with the **same root symbol**.

```zane
main.cursor.target = main.nodeB   // legal: both paths root at `main`
this.terminal.io = this.io        // legal: both paths root at `this`
hub.io = this.io                  // ILLEGAL: roots `hub` and `this` differ
```

The root may be a host or a guest. Everything reachable under one name belongs to one hosting tree ([`memory.md`](memory.md) §2.1), so a guest stored under that name travels with what it points at, whatever scope the tree comes to be hosted in, and goes when the tree does. That is what moves and raises would otherwise have broken, and all this rule claims. A host destroyed while its tree lives on is a separate matter, governed by §2.1 and by [`memory.md`](memory.md) §2.8.1.

What the rule refuses is the store whose two sides belong to different trees. There, the comparison §1.1 makes for an `&` symbol has nothing to compare: an `&` field lives with the object that holds it, and that object's host is not named at the store.

This is also what makes a swallowed parameter safe to mint a guest from. Binding one into `&` storage has different roots and is refused here, so the restriction in [`memory.md`](memory.md) §2.9 follows from this rule rather than standing on its own.

A verb that must install a guest into an object takes the host instead, moves it into a field, and points at the field:

```zane
Unit install(this Main, io std$IO) mut {
    this.io = io                 // move into the hosting field
    this.terminal.io = this.io   // legal: both paths root at `this`
    return Unit()
}
```

`init{ }` has no root to share, because the object does not exist until it completes. A constructor may therefore write an `&` field from one of its `&T` parameters, which is what [`types.md`](types.md) §3.9 already requires of it; §1.11 governs where the finished object may then go.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-check-that-fired-once-and-the-move-that-outran-it) — "The check that fired once, and the move that outran it".

### 1.11 Raising a value re-checks the guests it carries
A value **carries a guest** when an `&` is reachable from its type by following **owning** edges (see [`adt.md`](adt.md) §4). The walk finds an `&` member and stops at it: a type's own `&` field is the shortest case, reached after no edges at all, and a `&` nested inside a hosting field or container element is reached by following those edges to it. The walk does not continue *through* an `&` into what it names, because that object is hosted elsewhere and moves separately.

Raising such a value is legal only when every guest it carries names a host declared in the same or a higher lexical scope than the destination, or names a host **inside the value being raised**. A value is raised when:

- it moves into a host declared in a higher scope than its source host,
- it is a move-source with **no source host** — a hosting verb result or a `#variant` case form (§1.2) — bound into any host, where the destination is that host. §1.4 is satisfied trivially by such a value because it re-parents nothing; the guests it carries are a separate question, and the comparison is made against where it comes to rest,
- it is returned, where the destination is the call-site scope (§1.5), or
- it is passed as an argument, where the destination is the host of every other reference-type argument, and the return (§1.8). With no other reference-type argument and no returned host, the destination is the call-site scope (§1.5), which the argument already sits in — so nothing is raised and nothing is checked.

```zane
outerHolder Holder(Engine(Int(1)))
parked Car(outerHolder.engine)     // Car holds an `&Engine`
{
    innerHolder Holder(Engine(Int(2)))
    arriving Car(innerHolder.engine)
    parked = arriving              // ILLEGAL: the guest names a host in this block,
}                                  //   and parked is declared above it
```

The argument form is what makes the rule hold across a call. Neither frame sees the raise on its own — the argument reaches a parameter in the call-site scope, and inside the callee both parameters share that scope — so the comparison is made at the call site, against what the signature admits:

```zane
cars List<Car> = []
{
    innerHolder Holder(Engine(Int(2)))
    arriving Car(innerHolder.engine)
    cars!append(arriving)          // ILLEGAL: arriving's guest names a host in this
}                                  //   block, and cars is hosted above it
```

A value with no source host is checked the same way, against the host it is bound into. It re-parents nothing, so §1.4 waves it through; what it *carries* still has to reach the destination:

```zane
result Expr = Expr.intLit("0")
{
    innerTree Tree(Expr.intLit("5"))
    result = Expr.flip(innerTree.root)   // ILLEGAL: the case form carries a guest to
}                                        //   this block, and result is declared above it
```

A guest that names a host inside the raised value satisfies the rule at every destination, because it travels with the value. This is what a constructor's `init{ }` settles:

```zane
Main(io std$IO) => init{io, terminal = Terminal(io)}
```

`io` moves into the Main's own field, and the guest inside `terminal` follows it there ([`memory.md`](memory.md) §2.8.1). A `Main` may therefore be raised to any destination, while a `Car` holding a guest to storage it does not own may not.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-check-that-fired-once-and-the-move-that-outran-it) — "The check that fired once, and the move that outran it".

---

## 2. Lifetime and Destruction

### 2.1 Destruction is deterministic
Class instances are destroyed when their host dies, their hosting container dies, or their hosting scope drains under the concurrency rules.

A **value** has death points that are equally static: its slot is overwritten, or the host, container, or scope holding it dies. Whatever storage that value owns out of line — the payload of a boxed member, and every payload beneath it — is returned at that point, recursively (see [`memory.md`](memory.md) §2.3 and §3.2). No tracking is needed to find the moment, because every one of these points is known from the program text.

### 2.2 Scopes drain before destruction
If a scope launches concurrent work, objects hosted by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule (see [`concurrency.md`](concurrency.md) §4.1).

### 2.3 Guest storage never extends lifetime
Guests do not participate in hosting and cannot prolong object lifetime. They only track a live object whose host is already guaranteed to outlive them.

### 2.4 Null guests are not a user-facing state
Because the scope rules prevent guests from outliving their hosts, the runtime does not expose a normal “null guest” programming model to the user. Three rules together carry that guarantee: §1.1 compares scopes when an `&` symbol is assigned, §1.10 confines a stored `&` to the tree it is written through, and §1.11 re-checks the guests a value carries whenever it is raised.

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
| `&` return | Returned `&T` must be rooted in a parameter of either mode, `this` included, because a parameter belongs to the call-site scope; a local is not a root |
| Guest assignment | Only from a guest source ([`memory.md`](memory.md) §2.8) whose host is in the same or a higher lexical scope than the guest; a bare symbol is a guest source, a `[]` expression is not |
| Move-source | A direct host symbol (local or parameter), a hosting verb result, or a `#variant` case form; not an `&`, a value-type borrow, a field, a container element, or any other access path |
| Move declaration-block restriction | A direct host symbol may only be moved in the exact lexical block where it was declared; parameters may be moved at the body top level |
| Move destination scope | Destination host must be in the same or a higher lexical scope than the source host |
| `&` field assignment | Destination path and source path must begin with the same root symbol; `init{ }` is exempt, having no root yet |
| Raising a guest-carrying value | Every guest reachable along owning edges must name a host at or above the destination, or a host inside the value itself; a return raises to the call-site scope, an argument to every other reference-type argument's host and the return, and to the call-site scope when there is neither; a move-source with no source host is checked against the host it is bound into |
| Post-move downgrade | After a move, the source symbol downgrades to an `&` and remains readable but is no longer a move-source |
| Parameter scope | A reference parameter belongs to the call-site scope, not the body, so a value passed by hosting access outlives the call |
| Hosting argument | A verb takes a **guest** (`&T`, caller keeps it), **relays** the host (`T` and returns a hosting handle, caller may bind it to host again), or **consumes** it (`T`, no host returned, caller keeps a guest); passing to a plain `T` downgrades the caller to a guest whatever the body does |
| Return value | A return need not be bound; an unbound reference-type result floats to the enclosing scope as an anonymous host, while an ignored value-type result is discarded |
| Destruction | Deterministic and delayed until the hosting scope drains |

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#no-rule-to-spare-the-specific-hole-each-restriction-plugs) — "No rule to spare: the specific hole each restriction plugs".
