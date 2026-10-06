# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: the scope comparison every store makes, moves, and deterministic destruction. It builds on the owner and reference storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for ownership and storage, §4 for references. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 A store may not raise a value above what it names

Every place has a **scope**: the block whose lifetime bounds it.

- a **symbol** — a local binding — has the block that declares it as its scope
- a **field or element** reached from its root by **owning** steps takes that root symbol's scope, never one of its own. Every element of a container shares the container's scope, so which element it is does not enter the comparison.
- a **`^T` parameter** is an owner of the body, scoped to the body's top block ([`memory.md`](memory.md) §2.9).
- `this` and an **`&T` parameter**, and a constructor's `init{ }`, have no scope in the body. Each stands for a path in the caller's frame. A store through `this` or `init{ }` is settled at the call site (§1.11). Every other parameter, an `&T` parameter included, is read-only ([`effects.md`](effects.md) §4.1) and is never a destination.

A path that steps *through* an `&` leaves the tree its root names. What lies beyond belongs to a different tree whose root the path does not mention, so no scope can be computed for it and it is not a place this rule can govern. Such a path may be **read** freely; it may not be the destination of a store:

```zane
main.peer.io = someIO;  // ILLEGAL: `peer` is an `&`, so `main` does not name
                        //   the tree this would write into
```

A **store** is legal only when every owner the stored value names — directly, or through an `&` it **carries** (§1.10) — has a scope that outlives the destination's scope. An assignment, a move, a return, an abort, and an argument are all stores. There is one comparison in this section, and those are the places it is made.

Two clauses complete it. A **block** outlives every block nested within it, and a symbol's scope is fixed at that symbol's declaration, so nothing later can falsify it. And the owners **inside** a stored value travel with it, taking the destination's scope.

A block is one lifetime, not a sequence of them. Everything scoped to it dies when it drains (§2.1), with no user code interleaved and no order among them to observe, so two things in one scope can never see each other's death. That is why the comparison is between scopes rather than between declaration positions.

```zane
node Node();
r &Node = node;                 // legal: one scope holds both

outerTree Tree();
do() {
    r2 &Node = outerTree.root;  // legal: the outer block outlives this one
    innerTree Tree();
    r = innerTree.root;         // ILLEGAL: this block does not outlive r's
}
```

When a store must **mint** a new reference, its source must also be a settled reference source ([`memory.md`](memory.md) §2.8). That condition is independent of the scope comparison. A store whose source value is already `&T` copies that existing reference instead and does not reapply the minting restriction.

A field is **not** confined to its own tree. It inherits its root symbol's scope, so an object and what its `&` field names may be siblings in one block:

```zane
io IO();
terminal Terminal(io);  // legal: one scope holds terminal and io
```

That costs nothing while both sit there. A settled `terminal` never moves, so the comparison is made once. A roaming value moves, and every store of it runs the comparison again over the references it carries (§1.10). A store through a path that has **no** scope in this frame is the deferred case: `init{ }` fills an object whose destination the constructor cannot see, so the obligation is published in the signature and discharged by each caller (§1.11).

The comparison the compiler makes is between two declaration blocks, after resolving each place to its scope. It does not perform borrow inference or lifetime annotation solving.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#inheriting-a-debt-safety-without-a-borrow-checker) — "Inheriting a debt: safety without a borrow checker".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-owner-lifetime-replaces-the-same-root-rule) — "The owner lifetime replaces the same-root rule".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#place-lifetimes-inside-a-live-owner) — "Place lifetimes inside a live owner".
> **Story:** [`stories/memory.md`](../stories/memory.md#owner-and-reference-replace-host-and-guest-and-the-store-rule-compares-scopes) — "Owner and reference replace host and guest, and the store rule compares scopes".

### 1.2 Move-sources are roaming owners, `^T` results, and `#variant` case forms

A move-source must denote a **roaming value the expression is entitled to consume**. Four forms qualify:

- a **roaming owner symbol**: a local declared `^T`, or a `^T` parameter, named directly by an identifier expression
- a **field of a roaming root**, reached from such a symbol by field steps ([`memory.md`](memory.md) §2.8.1)
- a **verb result** of type `^T`: a value returned by a verb (function, method, operator, constructor, or lambda) that hands back a roaming owner. It has no source owner; its source scope is the producing expression, which is always nested within or equal to the destination's scope. A constructor's result is one.
- a **`#variant` case form**: `Variant.case(payload)` where `Variant` is a **reference** sum type (see [`adt.md`](adt.md) §3.2). It is built-in syntax rather than a verb, but it produces a fresh value nothing owns yet, and it is a move-source on the same terms. A *value* `variant` case form is not one, and does not need to be: a value sum is copied rather than owned ([`memory.md`](memory.md) §2.3).

A verb result and a case form produce a fresh value that no symbol, field, or container owns yet. Moving it transfers ownership of that temporary straight into the destination. This is what lets a recursive structure be written as one nested expression: each boxed owning member takes the node built for it in place (see [`adt.md`](adt.md) §4).

The following are **not** move-sources:

- a **settled** owner — a bare reference-type symbol, or a field of a settled root ([`memory.md`](memory.md) §2.1)
- an `&` value, including a verb that returns `&T`
- a borrow parameter or the subject `this` ([`memory.md`](memory.md) §2.9)
- a container element access such as `cars[1]`, or a variant case payload

```zane
engine ^Engine = Engine();
car Car(engine);              // legal: engine is a roaming symbol
boat Boat(makeEngine());      // legal: makeEngine() returns ^Engine

parked Engine = Engine();
truck Truck(parked);          // ILLEGAL: parked is settled
garage Garage(cars[1]);       // ILLEGAL: container element is not a move-source
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#what-may-be-moved-keeping-ownership-subtrees-whole) — "What may be moved: keeping ownership subtrees whole".
> **Story:** [`stories/memory.md`](../stories/memory.md#settled-and-roaming-the-host-that-stopped-moving) — "Settled and roaming: the host that stopped moving".

### 1.3 Moves are restricted to the declaration block

A roaming owner symbol, or a field of one, may only be used as a move-source in the exact lexical block where that symbol was declared. A `^T` parameter may be used as a move-source at the top level of the function body.

```zane
engine ^Engine = Engine();
car Car(engine);         // legal: same block as engine's declaration

do() {
    node ^Node = Node();
    innerOwner Node = node; // legal: same block as node's declaration
}
```

Moving an outer symbol from a nested block is illegal:

```zane
car ^Car = Car();
do() {
    garage Garage(car);  // ILLEGAL: car was declared in outer block
}
```

```zane
Unit loadCar(this Boat, car ^Car) mut {
    this.cars!append(car); // legal: car is moved into this.cars at the top level of the body
    return Unit();
}
```

This restriction prevents conditional moves and flow-dependent owner changes. If control flow is needed, compute the destination or the deciding condition first, then perform a single move in the symbol's declaration block. A store that refills a spent symbol is confined to the same block (§1.6).

The restriction applies only to symbol move-sources. A verb result or `#variant` case form (§1.2) is an unnamed temporary with no declaration block, so it is simply consumed at the point where it appears.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-declaration-block-rule-and-the-flow-analysis-it-refuses) — "The declaration-block rule, and the flow analysis it refuses".

### 1.4 A move needs no scope comparison of its own

A moved object is roaming, so nothing references it, and moving it strands nothing. A roaming symbol moves only in its declaration block (§1.3), so the owner it moves into is declared there or above. A settled owner never moves, so its scope is fixed where it settles. The only comparison a move makes is §1.1's, over the references the moved value **carries** (§1.10).

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#lifetime-rules-after-settled-and-roaming-hosts) — "Lifetime rules after settled and roaming hosts".

### 1.5 A borrow lasts for the call; a taken parameter is the body's

A reference-type parameter is one of three modes ([`memory.md`](memory.md) §2.9), and each has a fixed relation to the call:

- A **borrow** (`T`, and every subject) is the caller's owner, lent for the call. It is not stored, returned, or moved, so nothing of it outlives the call, and the caller's owner is untouched. Nothing else in the call writes it ([`memory.md`](memory.md) §2.9.1).
- A **take** (`^T`) moves the caller's roaming owner into the body. The parameter is then a roaming owner scoped to the body's top block (§1.1). The body moves it on — into another parameter's object, into the result, into a local — or it dies when the body drains (§2.1).
- A **reference** (`&T`) is the caller's reference, copied. It stands for the caller's path, so a store that reaches it is settled at the call site (§1.11).

```zane
Unit enterMatch(player ^Player) {
    island Island = makeIsland();
    island!startMatch(player); // player moves into the local island
    return Unit();
}
```

`startMatch` takes `player` into the local `island`, and `island` drains at the return, taking `player` with it. A body that means to keep the player alive hands it back through its result (§1.8), or moves it into an object the caller supplied.

For `&` fields specifically, the callee must declare the corresponding parameter as `&T`. A `^T` parameter is roaming and is never a reference source ([`memory.md`](memory.md) §2.8), and a borrow is never stored.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#consumed-or-borrowed-the-parameter-that-lives-at-the-call-site) — "Consumed or borrowed: the parameter that lives at the call site".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#lifetime-rules-after-settled-and-roaming-hosts) — "Lifetime rules after settled and roaming hosts".

### 1.6 A moved symbol is spent until a store refills it

After a roaming owner symbol is moved, it is **spent**: it denotes no object. Reading it, calling a method on it, passing it, or moving it again is a compile-time error. A spent symbol keeps its full storage, and a store into it **refills** it: the symbol then owns the stored object. Nothing references a roaming owner, so a refill is never observed by anything that watched the old object.

```zane
engine ^Engine = Engine();
car Car(engine);         // engine is moved; engine is spent
engine:inspect();        // ILLEGAL: engine is spent
engine = Engine();       // refills engine with a new object
engine:inspect();        // legal: engine owns the new object
```

Passing a roaming owner to a `^T` parameter is a move, so it spends the caller's symbol too (§1.8). A field moved out of a roaming root leaves that field spent in the same way, and the root is spent as a whole until every spent field is refilled.

A symbol changes between owning and spent only in the block where it is declared. A move out of it is confined there by §1.3, and a store that refills it is confined there too, so whether a symbol is spent never depends on which path ran. Overwriting a symbol that still owns an object leaves it owning one, so that store is not confined.

```zane
engine ^Engine = Engine();
car Car(engine);         // engine is spent
if(ready()) {
    engine = Engine();   // ILLEGAL: refills a spent symbol outside its declaration block
}
```

A parameter is never refilled. A store into one is a write, and a parameter is read-only ([`effects.md`](effects.md) §2.4). A body that needs an owner back after passing a `^T` parameter on moves the parameter into a local first, and refills that (§1.8).

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#downgrade-not-poison-why-there-is-no-use-after-move-read) — "Downgrade, not poison: why there is no use-after-move-read".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#a-moved-host-is-spent-not-a-guest) — "A moved host is spent, not a guest".

A verb result (§1.2) has no symbol to spend. The temporary is consumed by the move and cannot be named again, so the double-move question never arises for it.

### 1.7 Returned `&` values must be rooted in an `&T` parameter or a package constant

A return is a store into the call-site scope, so §1.1 governs it, and this is what the comparison comes to for a returned reference: a function may return an `&T` only when the returned reference is rooted in one of the function's **`&T` parameters** or in a **package constant** — the root used bare, or a field access whose base chain reaches it.

```zane
&Weapon weaponOf(player &Player) => player.weapon
```

An `&T` parameter stands for a path in the caller's frame (§1.5), so it has no scope the body could compare against. The obligation travels out with the signature and the call site discharges it against the argument path (§1.11), which is where the two scopes are finally both in view. A package constant lives for the whole program, so it outlives every caller's scope and needs no comparison at the call site:

```zane
garage Garage(...);                    // a package constant

&Engine spareEngine() => garage.spare  // legal: rooted in a package constant
```

Nothing else is a root. A **local** is excluded by lifetime: a body block does not outlive the call-site scope, so §1.1 rejects the store outright. A **`^T` parameter** is an owner of the body (§1.5), excluded the same way. A **borrow**, `this` included, is never returned at all ([`memory.md`](memory.md) §2.9).

```zane
&Node bad() {
    value Node();
    return value;  // ILLEGAL: value is declared in the body, which drains at the return
}
```

This rule governs a return that **is** an `&T`. A return that *carries* one — an owning value with an `&` reachable inside it — is the same store, and §1.1 compares the scope of the carried reference's owner on the same reasoning.

An `abort` is a store into the call-site scope exactly as a `return` is: its value lands in the caller's handler rather than in the caller's result ([`error-handling.md`](error-handling.md) §3). So an aborted `&T` is held to the same roots, and so is a reference carried by an aborted value:

```zane
Int?&Node refused() {
    value Node();
    abort value;  // ILLEGAL: value is declared in the body, which drains at the abort
}

Int?&Node passed(node &Node) {
    abort node;   // legal: rooted in an `&T` parameter
}
```

The handler's binder is then what the call's result would have been: it names what the call's argument paths name, and every store of it is compared against those (§1.11).

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#returning-a-ref-without-a-lifetime-to-name-it) — "Returning a ref without a lifetime to name it".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#simplifying-the-return-root-rule) — "Simplifying the return-root rule".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#package-constants-become-return-roots-and-an-t-parameter-stops-being-a-store-root) — "Package constants become return roots, and an `&T` parameter stops being a store root".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#running-the-examples) — "Running the examples".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#lifetime-rules-after-settled-and-roaming-hosts) — "Lifetime rules after settled and roaming hosts".

### 1.8 Passing a roaming owner to a `^T` parameter spends it

A `^T` parameter takes its argument by moving it. Passing a roaming owner symbol to one uses that symbol as a move-source (§1.2), so the caller's symbol is spent (§1.6) — **whatever the callee does with the value**. The parameter's declared type is the whole contract: `^T` means the caller gives the owner up; `T` and `&T` ([`memory.md`](memory.md) §2.9) leave the caller a full owner. Nothing in the callee's body changes the outcome the signature already states.

```zane
car ^Car = Car();
garage!store(car);    // store takes `^Car`: car is spent
car:inspect();        // ILLEGAL: car is spent
truck Truck(car);     // ILLEGAL: car is spent
```

A verb that takes an owner in one of three ways, each fixed by its signature:

- it **borrows** — declares the parameter `T`; the caller stays a full owner, and the callee may read it.
- it **relays** the owner — declares `^T` and returns `^T`; the caller's symbol is spent, and the symbol the return is bound to owns the object again.
- it **consumes** the owner — declares `^T` and returns no owner; the caller's symbol is spent, and the value stays wherever the verb placed it, or dies with the body.

A relay that takes a value and hands it back uses the return path. A parameter is read-only and is never refilled (§1.6), so the body moves `player` into a local first. `startMatch` consumes `kept` into `island`, so `kept` is spent; `enterMatch` then refills it from `returnPlayer`'s return, in `kept`'s own declaration block, so `kept` is an owner again and `return kept` is an ordinary move:

```zane
^Player enterMatch(player ^Player) {
    kept ^Player = player;                // the parameter moves into a local
    island Island = makeIsland();
    playerId Int = kept.id;
    island!startMatch(kept);              // startMatch consumes kept; kept is now spent
    kept = island!returnPlayer(playerId); // refill: kept is a full owner again
    return kept;
}

Unit main() {
    player ^Player = makePlayer();
    player = enterMatch(player);          // bind to regain the owner
    return Unit();
}
```

Because the signature alone decides the caller's state, there is no interprocedural consumption inference. The resting-place summary of §1.11 does not reopen this. It records **where** a parameter's reference comes to rest, which the caller needs in order to compare scopes; it never changes **whether** passing an owner spends the caller's symbol, which the declared mode fixes on its own. Leaving a parameter entirely unused is a separate, general matter — a release build rejects an unused parameter whatever its mode.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".
> **Story:** [`stories/memory.md`](../stories/memory.md#three-ways-to-hand-over-an-object) — "Three ways to hand over an object".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-borrow-comes-back-without-a-sigil) — "The borrow comes back, without a sigil".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#running-the-examples) — "Running the examples".

### 1.9 An ignored `^T` result is destroyed

A return value need not be bound. When a call's result is a roaming owner and the call stands as a bare statement, nothing owns the result and nothing references it, so it is destroyed at the end of the statement, with every block it holds. An ignored value-type result, including `Unit()`, is simply discarded.

```zane
car2 ^Car = repair(car);  // bind: car2 owns the result, and may move it on
repair(car3);             // legal: the result is destroyed here
```

Binding the return is how the caller keeps the owner. A relayed owner that is not bound is gone, which the caller can see at the call: a bare statement keeps nothing.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#lifetime-rules-after-settled-and-roaming-hosts) — "Lifetime rules after settled and roaming hosts".

### 1.10 A value carries the references reachable along owning edges

A value **carries a reference** when an `&` is reachable from its type by following **owning** edges (see [`adt.md`](adt.md) §4). The walk finds an `&` member and stops at it: a type's own `&` field is the shortest case, reached after no edges at all, and an `&` nested inside an owning field or container element is reached by following those edges to it. The walk does not continue *through* an `&` into what it names, because that object is owned elsewhere.

The owners a value's carried references name are what §1.1 compares alongside the value's owner. A roaming value's carried references name settled owners outside it, since nothing inside a roaming owner is referenceable ([`memory.md`](memory.md) §2.8.1). Each keeps the scope it has, and every store of the value asks again whether that scope outlives the new destination:

```zane
outerHolder Holder(Engine(Int(1)));
parked ^Car = Car(outerHolder.engine);   // Car holds an `&Engine`
do() {
    innerHolder Holder(Engine(Int(2)));
    arriving ^Car = Car(innerHolder.engine);
    parked = arriving;             // ILLEGAL: the reference names an owner scoped to this
}                                  //   block, and parked is scoped above it
```

The walk reads the **declared type**, not the value's current contents. For a `#variant` that means every case, because which case is live is the flow-sensitive fact §1.3 exists to refuse. That decides only whether a value *may* carry a reference. What a carried reference **names** is read from the value's construction, which §1.3 keeps in the same block as any move of it — so a case form that supplies no `&` names no owner, nothing is compared, and the store passes. No valid program is rejected for holding a case the walk had to consider.

A value with **no source owner** — a verb result or a `#variant` case form (§1.2) — is asked the same question, against the owner it is bound into:

```zane
type Expr = #variant {
    intLit String;
    ref &Node;      // an `&` payload, so this case form takes a reference source
}

result ^Expr = Expr.intLit("0");
do() {
    innerTree Tree();
    result = Expr.ref(innerTree.root);  // ILLEGAL: the case form carries a reference to
}                                       //   this block, and result is scoped above it
```

`innerTree.root` is a field of a settled root, which is a reference source ([`memory.md`](memory.md) §2.8) and so is what an `&` payload asks for. It would **not** do for an owning payload, which takes a move-source ([`adt.md`](adt.md) §3.2) — the two payload kinds ask for different things, and only the `&` kind produces a carried reference here.

A settled value may hold references into its own fields, wired after it settles ([`memory.md`](memory.md) §2.8.1). It never moves, so those references are compared once, where they are stored. What none of this reaches is an owner destroyed while its tree lives on, which §2.1 answers.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#an--store-compares-root-symbols) — "An `&` store compares root symbols".

### 1.11 A signature records where its parameters come to rest

An `&T` parameter, and `this`, have no scope in the body (§1.5), so a store that reaches one cannot be settled there. What the body settles instead is **where a value comes to rest**: when a verb stores an `&T` parameter, or a reference a `^T` parameter carries, into a place reachable from `this` or from the result, the parameter and the path it lands in are part of that verb's signature. Each call substitutes its own argument paths for the parameters and applies §1.1.

```zane
type Terminal = #struct {
    io &IO;       // an `&` field
}

type Main = #struct {
    terminal Terminal;   // an owning field
    peer &Terminal;      // an `&` field
    io IO;               // an owning field
}

Unit setIO(this Terminal, io &IO) mut {
    this.io = io;         // recorded: io comes to rest at this.io
    return Unit();
}
```

Both paths below resolve to `main`'s scope, because a field takes its root symbol's scope (§1.1) and both are reached from `main`:

```zane
main Main();
main.terminal!setIO(main.io);      // → main.terminal.io = main.io
                                    //   one scope holds both: legal
do() {
    ioInner IO();
    main.terminal!setIO(ioInner);  // → main.terminal.io = ioInner
}                                   //   ILLEGAL: this block does not outlive main's
```

A constructor is the same case. Its `init{ }` fills an object whose destination the body cannot see, so what the body can state is which parameters land in it:

```zane
Terminal(io &IO) => init{io;}       // recorded: io comes to rest at the result's io
```

```zane
main Main();
do() {
    ioInner IO();
    t ^Terminal = Terminal(ioInner);  // → t.io = ioInner; one scope holds both: legal
    main.terminal = t;                // ILLEGAL: t carries a reference scoped to this block,
}                                     //   and main is scoped above it
```

A `^T` parameter is recorded the same way for the references it carries, and that is what settles an argument carrying a reference. Neither frame sees the problem alone — the argument's reference names an owner in the call-site scope, and inside the callee the parameter lands in another parameter's object:

```zane
cars List<Car>;
do() {
    innerHolder Holder(Engine(Int(2)));
    arriving ^Car = Car(innerHolder.engine);
    cars!append(arriving);  // append records: value comes to rest in this's elements
}                           //   → ILLEGAL: arriving carries a reference scoped to this
                            //     block, and cars is scoped above it
```

The summary is **transitive**, in the way the effect summaries of [`effects.md`](effects.md) §5.2 are: a verb that hands a parameter to another verb inherits the resting places that call records for it. Without that, a reference could be laundered by passing it one frame further than the check looked.

```zane
Unit relay(this Terminal, io &IO) mut {
    this!setIO(io);       // recorded: io comes to rest at this.io, via setIO
    return Unit();
}
```

A recorded path begins at a **root** — `this` or the result — and continues with the same **owning** steps §1.1 resolves a place by: field selections, and "an element of" for a container. No index is recorded, because every element of a container shares its scope. What a path may not do is step *through* an `&` after its root, for the reason §1.1 gives — beyond that point the path has left the tree its root names.

```zane
Unit wire(this Main, io &IO) mut {
    this.terminal.io = io;  // recorded: `terminal` is an owning field of `this`
    this.peer.io = io;      // ILLEGAL: `peer` is an `&` mid-path (§1.1)
    return Unit();
}
```

A call **substitutes** the path the caller supplied — an argument path, or the path the result is bound into — for the root, keeps the recorded steps that follow it, and applies §1.1 to the place that results. The steps are preserved rather than collapsed, so `main!wire(main.io)` compares `main.terminal.io` against `main.io`, and two implementations that agree on the summary agree on the verdict.

The summary is derived from the body and published with the signature, so a call can be checked without the body in hand. A verb whose parameters come to rest nowhere records nothing, which is the common case; its calls need no substitution.

A call through a function value has no body to derive a summary from, and a function type records none ([`functions.md`](functions.md) §7.2). Such a call is checked as though every parameter came to rest in the result and, when the function type is `mut`, in `this`. Each `&T` argument, and each reference an argument carries, is then compared against the subject and against the place the result is bound into, wherever their types can hold a reference:

```zane
far Port(Int(1));
plug Plug(far);
wire Unit(this Plug, port &Port) mut {
    this.port = port;
    return Unit();
}
do() {
    near Port(Int(2));
    plug!wire(near);   // ILLEGAL: wire's type is `mut`, so near is taken to come to rest in plug
}
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-owner-lifetime-replaces-the-same-root-rule) — "The owner lifetime replaces the same-root rule".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-rejected-design-that-needed-no-signatures) — "The rejected design that needed no signatures".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#a-call-through-a-function-value-is-taken-to-store-every-reference) — "A call through a function value is taken to store every reference".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#package-constants-become-return-roots-and-an-t-parameter-stops-being-a-store-root) — "Package constants become return roots, and an `&T` parameter stops being a store root".

---

## 2. Lifetime and Destruction

### 2.1 Destruction is deterministic

A reference-type object is destroyed at one of three points, each known from the program text:

- its owner's **scope drains** without the object having moved elsewhere, which ends every owner scoped to it;
- its owner is **overwritten** ([`memory.md`](memory.md) §2.2);
- its place **disappears**: a list element is removed or a variant changes case, and the operation does not move the occupant out first.

A settled owner never moves, so it dies at its own scope's drain or at an overwrite. A roaming owner may move first, and dies wherever it last landed. An object in a disappearing place is roaming ([`memory.md`](memory.md) §2.8.1), so nothing references it and destroying it with the place leaves nothing to dangle.

A **value** has death points that are equally static: its slot is overwritten, or the owner, container, or scope holding it dies. Whatever storage that value owns out of line — the payload of a boxed member, and every payload beneath it — is returned at that point, recursively (see [`memory.md`](memory.md) §2.3 and §3.2). No tracking is needed to find the moment, because every one of these points is known from the program text.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#place-lifetimes-inside-a-live-owner) — "Place lifetimes inside a live owner".
> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#lifetime-rules-after-settled-and-roaming-hosts) — "Lifetime rules after settled and roaming hosts".

### 2.2 Scopes drain before destruction

If a scope launches concurrent work, objects owned in that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule (see [`concurrency.md`](concurrency.md) §4.1).

### 2.3 Reference storage never extends lifetime

References do not participate in ownership and cannot prolong an object beyond the lifetime fixed by its owner. A reference names a settled owner, and §1.1 keeps the reference from outliving it.

### 2.4 Null references are not a user-facing state

An `&` is never optional and is never tested for emptiness; the runtime exposes no “null reference” programming model to the user. A reference is initialized from a settled owner and names that owner until the reference dies. The owner never moves ([`memory.md`](memory.md) §2.1), and §1.1 compares scopes at every store, over the value's own references and over the references it carries (§1.10), deferring to the call site wherever a parameter stands in for a path it cannot see (§1.11).

---

## 3. Language Comparisons

### 3.1 Lifetime and destruction behavior

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Destruction timing | deterministic | non-deterministic | deterministic | manual / RAII |
| GC pauses | ❌ | ✅ | ❌ | ❌ |
| Dangling reference risk | ❌ | ❌ | ❌ | ✅ |
| Lifetime annotations | ❌ | ❌ | ✅ | ❌ |

---

## 4. Summary

| Concept | Rule |
|---|---|
| Store | Legal only when every owner the stored value names — its own, and every owner reached through a reference it carries — has a scope that outlives the destination's scope; an assignment, a move, a return, an abort, and an argument are all stores |
| Scope | A symbol's scope is its declaring block; a field or element reached by owning steps takes its root symbol's; a `^T` parameter's is the body's top block; `this`, an `&T` parameter, and a constructor's `init{ }` have none in the body and stand for a path in the caller's frame. A path stepping *through* an `&` has left its root's tree, has no scope, and may be read but never stored into. A block outlives every block nested in it |
| `&` return | Returned or aborted `&T` must be rooted in an `&T` parameter or a package constant; a local, a `^T` parameter, and a borrow, `this` included, are not roots |
| Reference assignment | Copies an existing `&T` value, or mints from a settled reference source ([`memory.md`](memory.md) §2.8): a bare settled symbol, or a path from a settled root or an `&T` parameter through struct fields and `ArrayRef` elements only |
| Move-source | A roaming owner symbol (local or `^T` parameter), a field of a roaming root, a `^T` verb result, or a `#variant` case form; not a settled owner, an `&`, a borrow, a container element, or a case payload |
| Move declaration-block restriction | A roaming owner symbol may only be moved in the exact lexical block where it was declared; `^T` parameters may be moved at the body top level |
| Move destination scope | Needs no comparison of its own: nothing references a moved owner, and a symbol moves only in its declaration block |
| Carried reference | A value carries every `&` reachable from its **declared** type along owning edges — for a `#variant`, across every case — stopping at each `&` rather than continuing through it; the type decides whether to look, the value's construction decides what is named. Each keeps its scope and is compared at every store of the value |
| Resting place | Where a verb stores an `&T` parameter, or a reference a `^T` parameter carries, is part of its signature: a path rooted at `this` or the result, continuing by owning steps only, never stepping through an `&`. Derived from the body, transitive through the calls the body makes, and published with the signature. A call substitutes the supplied path for the root, keeps the recorded steps, and applies the store rule to the result. A call through a function value takes every parameter to come to rest in the result and, under a `mut` function type, in `this` |
| Spent symbol | After a move, a roaming source symbol is spent: any use is a compile-time error until a store refills it, and it changes between owning and spent only in its declaration block; a parameter is read-only and is never refilled |
| Parameter modes | A borrow (`T`, and `this`) lasts for the call; a take (`^T`) moves the caller's roaming owner into the body, as an owner of the body; a reference (`&T`) stands for the caller's path |
| Owning argument | A verb **borrows** an owner (`T`, caller keeps it), **relays** it (`^T` and returns `^T`, caller may bind it to own the object again), or **consumes** it (`^T`, no owner returned); passing to `^T` spends the caller's symbol whatever the body does |
| Return value | A return need not be bound; an unbound `^T` result is destroyed at the end of its statement, and an ignored value-type result is discarded |
| Destruction | Deterministic: at the owner's scope drain, at an overwrite, or when a list element or variant payload disappears without being moved out; a settled owner dies only at its drain or an overwrite |

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#no-rule-to-spare-the-specific-hole-each-restriction-plugs) — "No rule to spare: the specific hole each restriction plugs".
