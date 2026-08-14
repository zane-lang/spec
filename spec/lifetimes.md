# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: the owner comparison every store makes, rehosting, and deterministic destruction. It builds on the host and guest storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for hosting and storage, §4 for anchors and tethers. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 A store may not raise a value above what it names
Every place has an **owner**, and an owner is a lifetime:

- a **symbol** — a local binding — is owned by the block that declares it
- a **field or element** reached from its root by **owning** steps is owned by that root symbol's owner, never its own. Every element of a container shares the container's owner, so which element it is does not enter the comparison.
- a **parameter**, `this` included, and a constructor's `init{ }` have no owner in the body. Each stands for a path in the caller's frame, so a store through one is settled at the call site (§1.11).

A path that steps *through* an `&` leaves the tree its root names. What lies beyond belongs to a different tree whose root the path does not mention, so no owner can be computed for it and it is not a place this rule can govern. Such a path may be **read** freely; it may not be the destination of a store:

```zane
main.peer.io = someIO   // ILLEGAL: `peer` is an `&`, so `main` does not name
                        //   the tree this would write into
```

A **store** is legal only when every host the stored value names — directly, or through an `&` it **carries** (§1.10) — has an owner that outlives the destination's owner. An assignment, a move, a return, and an argument are all stores. There is one comparison in this section, and those are the places it is made.

Two clauses complete it. A **block** outlives every block nested within it, and which block owns a symbol is fixed at that symbol's declaration, so nothing later can falsify it. And the hosts **inside** a stored value travel with it, taking the destination's owner — which is why a value may always be stored somewhere its own guests already point into.

A block is one lifetime, not a sequence of them. Everything it owns dies when it drains (§2.1), with no user code interleaved and no order among them to observe, so two things one block owns can never see each other's death. That is why the comparison is between owners rather than between declaration positions.

```zane
node Node()
r &Node = node                  // legal: one block owns both

outerTree Tree()
{
    r2 &Node = outerTree.root   // legal: the outer block outlives this one
    innerTree Tree()
    r = innerTree.root          // ILLEGAL: this block does not outlive r's
}
```

The source must also be a guest source ([`memory.md`](memory.md) §2.8). That condition is independent of the comparison, and nearly every place expression satisfies it, so in practice this rule is the owner comparison.

A field is **not** confined to its own tree. It inherits its root symbol's owner, so an object and what its `&` field names may be siblings in one block:

```zane
io IO()
terminal Terminal(io)   // legal: one block owns terminal and io
```

That costs nothing while both sit there, and the moment `terminal` is stored anywhere the comparison runs again — now against the new destination, and against the guest `terminal` carries (§1.10). A store through a path that has **no** owner in this frame is the deferred case: `init{ }` fills an object whose destination the constructor cannot see, so the obligation is published in the signature and discharged by each caller (§1.11).

The comparison the compiler makes is between two declaration blocks, after resolving each place to the block that owns it. It does not perform borrow inference or lifetime annotation solving.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#inheriting-a-debt-safety-without-a-borrow-checker) — "Inheriting a debt: safety without a borrow checker".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-root-rule-that-got-shorter) — "The root rule that got shorter".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#two-lifetimes-and-only-one-of-them-had-a-name) — "Two lifetimes, and only one of them had a name".

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
A move is a store, so §1.1 governs it. Read against the moved value's own host, the comparison says: a value may move into a new host only when the destination host is declared in the same or a higher lexical scope than the source host.

```zane
node Node()
{
    nestedOwner Node()
    nestedOwner = node // ILLEGAL: cannot move into a host declared in a nested scope
}
```

A hosting verb result (§1.2) has no source host; its source scope is the expression that produces it. That scope is always nested within or equal to the destination host's scope, so this reading is trivially satisfied and never blocks moving a verb result into any host. What such a value **carries** is a separate question, and §1.1 asks it against the host the value is bound into.

A parameter's value is exempt. Because a parameter belongs to the call-site scope and is not part of the body (§1.5), lending it into a local or a nested call does not sink hosting into that lower scope: the value returns to the call site when the local exits, unless the callee moves it into another parameter's hosting storage or into the return (§1.8).

This reading concerns the moved value's own host. When the value **carries guests**, §1.1 compares their owners too, and §1.10 says which guests those are.

### 1.5 Parameters belong to the call site
A reference-type parameter is **not part of the callee's body scope**. It behaves as a symbol in the **call-site scope**, one level above the body. Passing a hosting reference-type value to a plain `T` parameter lends it in with hosting access, but the value's lifetime stays with the call site.

Its **owner** (§1.1) is therefore no block of the body. A parameter stands for the argument path the caller wrote, which is why a store that reaches a parameter is settled by the call site rather than by the body (§1.11).

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
A return is a store into the call-site scope, so §1.1 governs it, and this is what the comparison comes to for a returned guest: a function may return an `&T` only when the returned guest is rooted in one of the function's **parameters** — the parameter used bare, or a field access whose base chain reaches it. `this` counts as a parameter for this rule.

```zane
&Weapon getWeapon(this Player) => this.weapon
```

Both parameter modes are roots, and for the same reason: a parameter belongs to the **call-site scope** (§1.5), never to the body, so it has no owner the body could compare against. The obligation travels out with the signature and the call site discharges it against the argument path (§1.11), which is where the two owners are finally both in view. A swallowing `T` parameter qualifies on exactly these terms — the value it took outlives the call (§1.5) — even though passing to it downgrades the caller (§1.8).

A **local** is the case this rule excludes, and it is excluded by lifetime rather than by what may mint a guest. A body block does not outlive the call-site scope, so §1.1 rejects the store outright:

```zane
&Node bad() {
    value Node()
    return value   // ILLEGAL: value is hosted by the body scope, which drains at the return
}
```

This rule governs a return that **is** an `&T`. A return that *carries* one — a hosting value with an `&` reachable inside it — is the same store, and §1.1 compares the carried guest's owner on the same reasoning.

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

- it takes a **guest** — declares the parameter `&T`; the caller stays a full host, and the callee may read it, mutate it, return it, or store it. Where a stored guest comes to rest is part of the signature (§1.11), and the caller's argument paths settle whether that store is legal (§1.1).
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

A verb that only reads its reference argument may still declare it plain `T`: reading does not change the fact that the signature asked for hosting access, so the caller downgrades all the same. Declaring the parameter `&T` is what keeps the caller as host. Because the signature alone decides the caller's state, there is no interprocedural consumption inference: whether a passed host downgrades never depends on the callee's body or on the build. The resting-place summary of §1.11 does not reopen this. It records **where** a parameter's value comes to rest, which the caller needs in order to compare owners; it never changes **whether** passing one downgrades the caller, which the declared mode fixes on its own. Using hosting access only to read a value is legal. Leaving a parameter entirely unused is a separate, general matter — a release build rejects an unused parameter whether it hosts a value or not.

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

### 1.10 A value carries the guests reachable along owning edges
A value **carries a guest** when an `&` is reachable from its type by following **owning** edges (see [`adt.md`](adt.md) §4). The walk finds an `&` member and stops at it: a type's own `&` field is the shortest case, reached after no edges at all, and an `&` nested inside a hosting field or container element is reached by following those edges to it. The walk does not continue *through* an `&` into what it names, because that object is hosted elsewhere and moves separately.

The hosts a value's carried guests name are what §1.1 compares alongside the value's own host. A guest naming a host **inside** the value is satisfied at every destination, because that host travels with it. A guest naming anything else keeps the owner it has, and every store of the value asks again whether that owner outlives the new destination:

```zane
outerHolder Holder(Engine(Int(1)))
parked Car(outerHolder.engine)     // Car holds an `&Engine`
{
    innerHolder Holder(Engine(Int(2)))
    arriving Car(innerHolder.engine)
    parked = arriving              // ILLEGAL: the guest names a host owned by this
}                                  //   block, and parked is owned above it
```

The walk reads the **declared type**, not the value's current contents. For a `#variant` that means every case, because which case is live is the flow-sensitive fact §1.3 exists to refuse. That decides only whether a value *may* carry a guest. What a carried guest **names** is read from the value's construction, which §1.3 keeps in the same block as any move of it — so a case form that supplies no `&` names no host, nothing is compared, and the store passes. No valid program is rejected for holding a case the walk had to consider.

A value with **no source host** — a hosting verb result or a `#variant` case form (§1.2) — is asked the same question, against the host it is bound into. It re-parents nothing, so §1.4 waves it through; what it carries still has to reach the destination:

```zane
type Expr = #variant {
    intLit String;
    ref &Node;      // an `&` payload, so this case form takes a guest source
}

result Expr = Expr.intLit("0")
{
    innerTree Tree()
    result = Expr.ref(innerTree.root)   // ILLEGAL: the case form carries a guest to
}                                       //   this block, and result is owned above it
```

`innerTree.root` is a field access, which is a guest source ([`memory.md`](memory.md) §2.8) and so is what an `&` payload asks for. It would **not** do for a hosting payload, which takes a move-source ([`adt.md`](adt.md) §3.2) — the two payload kinds ask for different things, and only the `&` kind produces a carried guest here.

A guest naming inside the value is what a constructor's `init{ }` normally settles:

```zane
Main(io std$IO) => init{io, terminal = Terminal(io)}
```

`io` moves into the Main's own field, and the guest inside `terminal` follows it there ([`memory.md`](memory.md) §2.8.1). A `Main` may therefore be stored anywhere, while a `Car` holding a guest to storage it does not own may only go where that storage outlives it.

Everything reachable under one root symbol belongs to one hosting tree ([`memory.md`](memory.md) §2.1), which is why a guest that names inside its own value needs no further comparison: it travels with what it points at and goes when the tree goes. What none of this reaches is a host destroyed while its tree lives on — a separate matter, governed by §2.1 and by [`memory.md`](memory.md) §2.8.1.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-check-that-fired-once-and-the-move-that-outran-it) — "The check that fired once, and the move that outran it".

### 1.11 A signature records where its parameters come to rest
A parameter has no owner in the body (§1.5), so a store that reaches one cannot be settled there. What the body settles instead is **where the value comes to rest**: when a verb stores a parameter into a place reachable from another parameter or from the result, the parameter and the path it lands in are part of that verb's signature. Each call substitutes its own argument paths for the parameters and applies §1.1.

```zane
type Terminal = #struct {
    io &IO;       // an `&` field
}

type Main = #struct {
    terminal Terminal;   // a hosting field
    peer &Terminal;      // an `&` field
    io IO;               // a hosting field
}

Unit setIO(this Terminal, io &IO) mut {
    this.io = io          // recorded: io comes to rest at this.io
    return Unit()
}
```

Both paths below resolve to `main`'s owner, because a field takes its root symbol's owner (§1.1) and both are reached from `main`:

```zane
main Main()
main.terminal!setIO(main.io)       // → main.terminal.io = main.io
                                    //   one block owns both: legal
{
    ioInner IO()
    main.terminal!setIO(ioInner)   // → main.terminal.io = ioInner
}                                   //   ILLEGAL: this block does not outlive main's
```

A constructor is the same case. Its `init{ }` fills an object whose destination the body cannot see, so what the body can state is which parameters land in it:

```zane
Terminal(io &IO) => init{io}       // recorded: io comes to rest at the result's io
```

```zane
main Main()
{
    ioInner IO()
    t Terminal(ioInner)   // → t.io = ioInner; one block owns both: legal
    main.terminal = t     // ILLEGAL: t carries a guest owned by this block,
}                         //   and main is owned above it
```

A swallowed `T` parameter is recorded the same way, and that is what settles an argument carrying a guest. Neither frame sees the problem alone — the argument reaches a parameter in the call-site scope, and inside the callee both parameters share it:

```zane
cars List<Car> = []
{
    innerHolder Holder(Engine(Int(2)))
    arriving Car(innerHolder.engine)
    cars!append(arriving)   // append records: car comes to rest in this's elements
}                           //   → ILLEGAL: arriving carries a guest owned by this
                            //     block, and cars is owned above it
```

The summary is **transitive**, in the way the effect summaries of [`effects.md`](effects.md) §5.2 are: a verb that hands a parameter to another verb inherits the resting places that call records for it. Without that, a guest could be laundered by passing it one frame further than the check looked.

```zane
Unit relay(this Terminal, io &IO) mut {
    this!setIO(io)        // recorded: io comes to rest at this.io, via setIO
    return Unit()
}
```

A recorded path begins at a **root** — a parameter, or the result — and continues with the same **owning** steps §1.1 owns a place by: field selections, and "an element of" for a container. No index is recorded, because every element of a container shares its owner. The root itself may be an `&T` parameter, which is an ordinary root like any other; what a path may not do is step *through* an `&` further along, for the reason §1.1 gives — beyond that point the path has left the tree its root names.

```zane
Unit setNested(target &Terminal, io &IO) mut {
    target.io = io          // recorded: io comes to rest at target.io
    return Unit()
}

Unit wire(this Main, io &IO) mut {
    this.terminal.io = io   // recorded: `terminal` is a hosting field of `this`
    this.peer.io = io       // ILLEGAL: `peer` is an `&` mid-path (§1.1)
    return Unit()
}
```

A call **substitutes** the path the caller supplied — an argument path, or the path the result is bound into — for the root, keeps the recorded steps that follow it, and applies §1.1 to the place that results. The steps are preserved rather than collapsed, so `setNested` called as `outer!setNested(main.terminal, main.io)` compares `main.terminal.io` against `main.io`, and two implementations that agree on the summary agree on the verdict.

The summary is derived from the body and published with the signature, so a call can be checked without the body in hand. A verb whose parameters come to rest nowhere records nothing, which is the common case; its calls need no substitution.

For an `&` field the callee must still declare the corresponding parameter `&T` ([`memory.md`](memory.md) §2.9, [`types.md`](types.md) §3.9). A swallowed value is hosted at the call site, so binding one into `&` storage would leave the field naming storage the caller may move out from under it, and no argument path the caller could supply would fix that.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#two-lifetimes-and-only-one-of-them-had-a-name) — "Two lifetimes, and only one of them had a name".

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
An `&` is never optional and is never tested for emptiness; the runtime exposes no “null guest” programming model to the user. One rule keeps a stored guest pointing at something live as values move: §1.1 compares owners at every store, over the value's own host and over the guests it carries (§1.10), deferring to the call site wherever a parameter stands in for a path it cannot see (§1.11). What that covers is **relocation** — a value travelling away from what its guests name. A host destroyed while its tree lives on is the separate question §2.1 and [`memory.md`](memory.md) §2.8.1 answer.

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
| Store | Legal only when every host the stored value names — its own, and every host reached through a guest it carries — has an owner that outlives the destination's owner; an assignment, a move, a return, and an argument are all stores |
| Owner | A symbol is owned by its declaring block; a field or element reached by owning steps by its root symbol's owner; a parameter and a constructor's `init{ }` have none in the body and stand for a path in the caller's frame. A path stepping *through* an `&` has left its root's tree, has no owner, and may be read but never stored into. A block outlives every block nested in it; hosts inside a stored value travel with it and take the destination's owner |
| `&` return | Returned `&T` must be rooted in a parameter of either mode, `this` included, because a parameter belongs to the call-site scope; a local is not a root |
| Guest assignment | Only from a guest source ([`memory.md`](memory.md) §2.8); a bare symbol is a guest source, a `[]` expression is not |
| Move-source | A direct host symbol (local or parameter), a hosting verb result, or a `#variant` case form; not an `&`, a value-type borrow, a field, a container element, or any other access path |
| Move declaration-block restriction | A direct host symbol may only be moved in the exact lexical block where it was declared; parameters may be moved at the body top level |
| Move destination scope | Destination host must be in the same or a higher lexical scope than the source host — the store rule read against the moved value's own host |
| Carried guest | A value carries every `&` reachable from its **declared** type along owning edges — for a `#variant`, across every case — stopping at each `&` rather than continuing through it; the type decides whether to look, the value's construction decides what is named. One naming a host inside the value satisfies any destination, one naming anything else keeps its owner and is compared at every store. Carrying none skips this comparison only, never the value's own host |
| Resting place | Where a verb stores a parameter is part of its signature: a path rooted at another parameter or at the result, continuing by owning steps only, never stepping through an `&`. Derived from the body, transitive through the calls the body makes, and published with the signature. A call substitutes the supplied path for the root, keeps the recorded steps, and applies the store rule to the result. It records where a parameter lands, never whether passing one downgrades the caller |
| Post-move downgrade | After a move, the source symbol downgrades to an `&` and remains readable but is no longer a move-source |
| Parameter scope | A reference parameter belongs to the call-site scope, not the body, so a value passed by hosting access outlives the call |
| Hosting argument | A verb takes a **guest** (`&T`, caller keeps it), **relays** the host (`T` and returns a hosting handle, caller may bind it to host again), or **consumes** it (`T`, no host returned, caller keeps a guest); passing to a plain `T` downgrades the caller to a guest whatever the body does |
| Return value | A return need not be bound; an unbound reference-type result floats to the enclosing scope as an anonymous host, while an ignored value-type result is discarded |
| Destruction | Deterministic and delayed until the hosting scope drains |

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#no-rule-to-spare-the-specific-hole-each-restriction-plugs) — "No rule to spare: the specific hole each restriction plugs".
