# Zane Memory Model

This document specifies Zane's memory model: hosting, guests, and arena layout. Lexical lifetime rules, moves, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for value and reference types. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling guests by combining single hosting, a host that never moves once anything may point at it, and lexical lifetime rules.

- **`Settled and roaming hosts`.** A reference-type host is either **settled** — it may be guested, and it never moves again — or **roaming** — it may move anywhere, and nothing guests it. A roaming host settles by moving into a settled place (§2.1, §2.8.1).
- **`Guests name settled hosts`.** An `&` — a **guest** — is a non-hosting handle to a settled host of a **reference type** (a `#`-marked type, or a reference-type intrinsic such as `@primitives$List<T>`). A value type has no identity to point at, so it is shared by copy or borrow, never by a stored guest (§2.4).
- **`A value copy is deep`.** A value owns whatever it holds out of line, so copying one copies its backing stores and boxed payloads into fresh storage instead of sharing them (§2.3, §2.10).
- **`Three passing modes`.** A bare reference-type parameter, and every subject, is a **borrow**; `^T` takes the host; `&T` takes a guest. A value-type parameter is always a borrow (§2.9).
- **`Lexical lifetime enforcement`.** Every store is checked against declaration scopes alone (see [`lifetimes.md`](lifetimes.md) §1), and objects are destroyed when their hosting scope drains; there is no tracing garbage collector (see [`lifetimes.md`](lifetimes.md) §2).
- **`Regioned arena placement`.** Every scope owns separate fixed-size and dynamic regions. Statically sized storage is placed inline in the fixed-size region; resizable data and the payloads of boxed members use the dynamic region (§3).
- **`A guest is an address`.** A settled host never moves, so a guest stores the host's segmented offset directly (§4).

The source language uses two words for the relationship: an object lives in a **host**, and a **guest** (`&T`) may access it without storing it or controlling its lifetime. A guest may point only at a settled host, and a settled host stays where it is until its scope drains, so a guest never needs to follow anything.

> **Story:** [`stories/memory.md`](../stories/memory.md#safety-without-a-collector-and-without-lifetimes) — "Safety without a collector and without lifetimes".
> **Story:** [`stories/memory.md`](../stories/memory.md#settled-and-roaming-the-host-that-stopped-moving) — "Settled and roaming: the host that stopped moving".

---

## 2. Hosting and Storage

### 2.1 Every reference-type instance has exactly one host, settled or roaming

Every instance of a reference type (a `#`-marked type, see [`types.md`](types.md) §2.1) is hosted by exactly one symbol, field, element, or payload at a time. A host is in one of two states:

- A **settled** host may be guested (§2.8). It never moves: no expression takes its object out of it.
- A **roaming** host may be moved (see [`lifetimes.md`](lifetimes.md) §1.2). Nothing guests it, or anything inside it.

A symbol, parameter, or return type written with `^` is roaming; a bare symbol of a reference type is settled. A field or an `ArrayRef` element takes the state of the root it is reached from, and a list element or variant payload is always roaming (§2.8.1). A roaming host **settles** when it moves into a settled place, and a settled host never becomes roaming.

```zane
spare ^Engine = Engine(Int(1));  // roaming
engine Engine = spare;           // settles here; spare is spent
view &Engine = engine;           // legal: engine is settled
moved Engine = engine;           // ILLEGAL: a settled host never moves
```

> **Story:** [`stories/memory.md`](../stories/memory.md#settled-and-roaming-the-host-that-stopped-moving) — "Settled and roaming: the host that stopped moving".

### 2.2 A settled host is overwritten in place

Any hosting storage position for a reference-type instance — a symbol, field, element, or payload — **MUST** be directly initialized, and **MAY** later be overwritten.

```zane
tank Tank(...);
tank = Tank(...); // legal
```

Overwriting a settled host destroys the old occupant and writes the replacement at the same address. This holds at every depth a guest can reach: each boxed member reached from the host through struct fields and `ArrayRef` elements (§3.3) is written into the block the occupant's member already holds, recursively, so no such member moves to a new block (§3.6). A variant payload is not followed, since nothing guests into one (§2.8.1). A fresh replacement is constructed directly in that storage; a moved-in replacement is copied into it, and the blocks that held its boxed members are returned. A guest to the host, or to any field of it, inline or boxed, therefore observes the replacement.

```zane
car Car(...);
r &Engine = car.engine;
car.engine = Engine(); // the old engine is destroyed; r observes the replacement
```

Overwriting a roaming host, a list element, or a variant payload destroys the old occupant unless the operation first moves it elsewhere. Nothing guests it, so nothing observes the change of occupant.

An `&T` stored *as an element value* is different: rewriting that element merely replaces one guest value with another.

> **Story:** [`stories/memory.md`](../stories/memory.md#settled-overwrites-stay-in-place-and-only-an-escape-relocates) — "Settled overwrites stay in place, and only an escape relocates".

### 2.3 Value types are copied whole, mutable in place, and freely overwritable

Value types have no identity. A value is mutated in place through a `mut` method whose `this` is a borrow of the value's storage (see [`effects.md`](effects.md) §2.3, [`functions.md`](functions.md) §2.4), and its storage slot may also be reassigned wholesale.

```zane
pos Vec2(1.0, 2.0);
pos!setX(Float(3.0)); // in-place field write through a borrow of pos
pos = Vec2(3.0, 4.0);  // whole-slot overwrite
```

A value-producing expression initializes storage according to whether it denotes an existing value. A **place expression** (§2.8) denotes existing storage; binding its value into a different slot copies the whole value. A **non-place expression** produces a fresh value and **MUST** construct that value directly in its eventual destination rather than first materializing an independent temporary and then copying it. This rule passes the destination recursively through nested value-producing forms: product construction, value-variant case forms, function results, `match` arms, and other fresh results build their members directly in the storage that will own them.

```zane
v Vector2 = Vector2(Int(3), Int(4)); // constructs v, v.x, and v.y directly
w Vector2 = v;                       // copies the existing value in v
```

A value-type parameter is a read-only borrow rather than a copy (§2.9), so passing one costs nothing. Binding through that borrow into fresh storage is a copy because the parameter denotes the caller's existing place. Where a copy does happen, it copies the **whole value**, including any storage that value owns. For a value whose members are all laid out inline, that is a copy of its inline bytes and nothing more; this is every value type that owns neither a backing store nor a boxed member (§2.10, §3.3). A value that owns out-of-line storage is copied **deeply**: the copy allocates a block for each backing store or boxed payload and copies its live contents into it, recursively, so the original and the copy share no storage at all. This includes `String` and `@primitives$String` ([`types.md`](types.md) §2.7).

Depth is not an extra feature bolted onto the copy; it is what the ordinary meaning of "copied" requires once a value may own out-of-line storage. A value's central promise is that nothing reachable from it is reachable from anywhere else, and a shallow copy would break exactly that by leaving two values naming one payload.

The cost is real and is accepted: copying such a value allocates and takes time proportional to its owned bytes and structure, where copying a flat value is one fixed-size write. Fresh construction does not pay that copy cost merely because its result is nested: `Countdown.more(Countdown.more(Countdown.done(Unit())))` constructs each node once in its final owning payload rather than repeatedly copying each completed prefix. Where a copied place is never read again — the argument a callee stores was the caller's last use of it — an implementation may move it instead of copying, since a value has no identity by which the two could be told apart.

An overwrite evaluates its right-hand side against the destination's **pre-overwrite** state. If the source is the destination itself or any place reached through it, the replacement value **MUST** be completely copied or otherwise materialized before the old occupant is destroyed and its owned blocks are returned. This makes `x = x`, `x = x.child`, and equivalent overlapping forms safe. An implementation may construct a non-place replacement directly in the destination slot when it proves that doing so preserves this order; the semantic rule does not require an observable temporary.

Allocation is no more a language-visible failure mode here than it is when a `List` outgrows its backing store (§3.6).

Destruction is the mirror. When a value dies — its host dies, its container dies, its scope drains, or its slot is overwritten (see [`lifetimes.md`](lifetimes.md) §2.1) — every block it owns is returned, recursively (§3.2).

> **Story:** [`stories/memory.md`](../stories/memory.md#what-a-copy-is-for-and-the-ban-that-survived-it) — "What a copy is for, and the ban that survived it".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-borrow-comes-back-without-a-sigil) — "The borrow comes back, without a sigil".

### 2.4 `&` is a guest: non-hosting storage

`&` creates a **guest**: non-hosting storage that points at a settled host of a **reference type**. An `&T` requires `T` to be a reference type — a declared `#struct`/`#variant`/`#enum`, or a reference-type intrinsic such as `@primitives$List<T>` — because only a reference type has a host to point at. A value type is shared by copying it or by a borrow (see [`functions.md`](functions.md) §2.4), never by a stored guest. Writing `&Node` names a guest to a reference type; a bare `&Int` over a value type is ill-formed.

An explicitly declared `&T` slot is **guest-only**: it stores only the guest and can never host a `T`. A slot declared as `T` or `^T` is **host-capable**. After a roaming host's value moves out, that same full-size slot is **spent** ([`lifetimes.md`](lifetimes.md) §1.6): it denotes no object until a store refills it.

A guest may be declared as:

- a local symbol
- a reference-type field
- an element type inside another storage type
- a function or constructor parameter
- a function return type

An `&` type is legal in storage sites (local symbols, fields, nested storage types), function parameter positions, and function return-type positions.

Declaring an `&` symbol is legal; §2.8 governs what may initialize it.

> **Story:** [`stories/memory.md`](../stories/memory.md#two-vocabularies-host-and-guest-above-anchor-and-tether) — "Two vocabularies: host and guest above anchor and tether".

### 2.5 Guests are repointable

An `&` symbol or `&` field may be assigned a different guest later, either by copying an existing `&T` value or by minting one from a settled place (§2.8), as long as the store rule in [`lifetimes.md`](lifetimes.md) §1.1 is satisfied. For an `&` **field or element**, the owner that rule compares is the field's root symbol's, not the field's own.

### 2.6 Guests are independent

Assigning or passing a guest gives the destination its own guest to the same host. Rebinding one guest's storage site later changes only that storage site; it does not retarget other guests that already point to that host.

### 2.7 Guests and hosts use the same surface operations

At use sites, a guest is used with the same surface syntax as a direct host. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between host and guest matters only at the storage site: a guest stores a non-hosting link, while a host stores the object itself.

### 2.8 Place expressions and new `&` values

A **place expression** is an expression that denotes an existing storage location.

The following are place expressions:

- a named local, field-backed, or hosting/`&` storage symbol such as `engine`
- a field access whose base is a place, such as `car.engine` or `this.engine`
- a subscript expression `list[index]` when `list` is a place expression and `[]` is defined as a place projection for that subject type
- an `&T` guest parameter inside the callee body (§2.9)

Only a **settled** place may mint a new guest. A new `&` value may be minted from:

- a **bare settled symbol** — a local or a package constant
- a path from a settled root that passes only through struct fields and `ArrayRef` elements, such as `car.engine` or `squad[2].weapon`
- an `&T` parameter, or such a path from one

Four things are rejected:

- A roaming host, and any place reached from one, is never a guest source. Nothing guests a host that may still move.
- A subscript is a guest source only when the place it projects is an `ArrayRef` element ([`functions.md`](functions.md) §2.9). A list element is roaming, so `players[100]` and `players[100].weapon` on a `List` are both excluded.
- A variant case payload is never a guest source, and neither is a path that continues through one.
- Temporaries and other value-only expressions are not place expressions at all. Constructor calls and ordinary function results such as `Engine()` and `makeEngine()` are not places.

```zane
engine &Engine = Engine();  // ILLEGAL: Engine() is a temporary, not a place expression
```

```zane
car Car();
r &Engine = car.engine;  // legal: a field of a settled root
s &Car = car;            // legal: a bare settled symbol
```

```zane
spare ^Car = Car();
t &Car = spare;          // ILLEGAL: spare is roaming
```

```zane
weapon &Weapon = players[100].weapon;  // ILLEGAL: players is a List; its elements are roaming
```

```zane
squad ArrayRef([Player(), Player()]);
lead &Weapon = squad[1].weapon;        // legal: squad is settled, and its elements are too
```

For a list element or a case payload, keep the guest at the settled container and perform the access through it when needed. A guest to a container may subscript that container, and a guest to a variant may read whichever case is live; neither access may mint a new guest to the element or case payload.

Reading an `&T` value that is already stored behind such an access remains legal:

```zane
armory Armory();
weapons List<&Weapon> = [armory.primary, armory.backup];
current &Weapon = weapons[1];  // legal: reads an `&Weapon` already stored in the list
```

The last line copies an existing guest value; it does not mint a new `&` from an element.

Non-`&` host bindings may be initialized from any expression, including temporaries. The host materializes the value into its storage.

```zane
engine Engine();        // legal: plain host binding; Engine() temporary is materialized into engine
```

> **Story:** [`stories/memory.md`](../stories/memory.md#contingent-hosts-float-to-their-owner) — "Contingent hosts float to their owner".
> **Story:** [`stories/memory.md`](../stories/memory.md#arrayref-a-fixed-reference-container-whose-elements-can-be-guested) — "`ArrayRef`: a fixed reference container whose elements can be guested".

### 2.8.1 A roaming host settles where it lands

A roaming host settles by moving into a settled place: a settled symbol, or a field of a settled root. The move is the last time the object moves. From then on it may be guested, and every guest to it names the same address until its scope drains ([`lifetimes.md`](lifetimes.md) §2.1).

```zane
^Car make(power Int) {
    car ^Car = Car(Engine(power));
    car.engine!tune();         // a roaming host is used like any other
    return car;                // and moves
}

parked Car = make(Int(1));     // settles here
garage.car = make(Int(2));     // settles in garage.car when garage is settled
```

What a host contains takes a state from where it sits:

- **Fixed storage inherits its root's state.** A struct's fields and an `ArrayRef`'s elements are settled under a settled root and roaming under a roaming one: they are fixed in number and all initialized at construction ([`generics.md`](generics.md) §8.4). An `Array` is a value type ([`generics.md`](generics.md) §8.1), so it holds no host at all (§2.10).
- **Dynamic storage is always roaming.** A list's elements and a variant's payload come and go while their owner lives, so they are roaming even under a settled root. A settled list may be guested as a whole; its elements may not.

A field is never declared roaming. An `ArrayRef` element is never moved out, under either kind of root: its index is a runtime value, so which element is spent could not be tracked. A field of a **roaming** root may be moved out, because nothing can observe the root; the root is then partly spent, tracked in its declaration block as a spent symbol is ([`lifetimes.md`](lifetimes.md) §1.6). A field of a settled root is overwritten, never moved out (§2.2).

A roaming value cannot hold a guest into its own insides, because nothing inside a roaming host is guestable. An object is wired to its own parts after it settles, from outside it:

```zane
kit Kit(Engine(Int(1)), Mount());
kit.mount!attach(kit.engine);  // legal: kit and its fields are settled
```

The `&` fields a roaming value holds may name settled hosts elsewhere; every store of the value compares them against its destination ([`lifetimes.md`](lifetimes.md) §1.10).

> **Story:** [`stories/memory.md`](../stories/memory.md#settled-and-roaming-the-host-that-stopped-moving) — "Settled and roaming: the host that stopped moving".
> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-new-ref-may-come-from) — "Where a new ref may come from".
> **Story:** [`stories/memory.md`](../stories/memory.md#arrayref-a-fixed-reference-container-whose-elements-can-be-guested) — "`ArrayRef`: a fixed reference container whose elements can be guested".

### 2.9 Function parameters: borrow, take, and guest

A **reference type** parameter has three passing modes, one per surface form:

| Mode | Written | Caller supplies | The callee may |
|---|---|---|---|
| Borrow | `T` | any host, settled or roaming, or a temporary | read it; never write it, store it, return it, or move it |
| Take | `^T` | a roaming host, which is spent, or a temporary ([`lifetimes.md`](lifetimes.md) §1.2) | move it, store it, or return it; it dies with the body otherwise |
| Guest | `&T` | a settled place that mints a guest, or an existing `&T` value (§2.8) | read it, return it as `&T`, or store it; where a stored guest comes to rest is part of the signature ([`lifetimes.md`](lifetimes.md) §1.11) |

- A **borrow** is non-hosting, non-escaping access to the caller's host for the duration of the call. It has no address the callee could keep: a borrow cannot be stored, returned, or minted into a guest. Like every parameter other than `this`, it is read-only ([`effects.md`](effects.md) §2.4), so it is never assigned or the subject of a `!` call; the subject is the one borrow a `mut` method may write. The caller stays a full host.
- A **take** moves the host into the callee. The parameter is a roaming host of the body; the body moves it on — into another parameter's object, into the result, into a local — or it dies when the body's scope drains.
- A **guest** parameter is an ordinary guest. Inside the callee body the parameter acts as a place expression that may be read or returned as `&T` under [`lifetimes.md`](lifetimes.md) §1.7. Binding it into an `&` **field** is decided at each call: the callee records that the parameter comes to rest in that field ([`lifetimes.md`](lifetimes.md) §1.11), and each call compares the owners of the two argument paths it actually wrote.

```zane
Float topSpeed(engine Engine) => engine.speed      // borrow

engine Engine();
s Float = topSpeed(engine);  // legal: engine stays a full host
spare ^Engine = Engine();
t Float = topSpeed(spare);   // legal: a borrow takes a roaming host too
```

A **value type** parameter has one mode, the borrow: a read-only borrow of the caller's slot for the duration of the call. A borrow is not storage, but that restriction is on the borrow, not on what is read through one. Binding through a borrow into a fresh slot (an assignment, a new declaration, or a field or return store) **copies** the value (§2.3). The copy outlives the call perfectly well; what does not escape is the borrow. Neither `^` nor `&` is written on a value-type parameter. A type parameter written `^T` takes a host when `T` is a reference type, and is a borrow when `T` is a value type.

Passing a value by borrow is the semantic model rather than an optimization; where a read-only borrow is indistinguishable from a copy, the compiler may still pass a small value by copy, the same latitude placement has (§3.5). The distinction becomes observable under concurrent sharing, where a spawned reader sees the borrowed value live (see [`concurrency.md`](concurrency.md) §4.4).

```zane
type Car = #struct {
    engine &Engine;   // an `&` field
    spare Engine;     // a hosting field
    _value Int;
}

// a take: the engine moves into a hosting field of this
Unit setSpare(this Car, engine ^Engine) mut {
    this.spare = engine;
    return Unit();
}

// a borrow, used only to read
Int inspect(this Car, engine Engine) {
    return this._value + engine.speed;
}

// a guest stored into an `&` field: the signature records where it lands
Unit setEngine(this Car, engine &Engine) mut {
    this.engine = engine;
    return Unit();
}
```

`setEngine` stores a guest it was handed. The callee sees two parameters and cannot tell whether the caller's `engine` is hosted above or below the object `this` names, so it does not decide: its signature records that `engine` comes to rest at `this.engine` ([`lifetimes.md`](lifetimes.md) §1.11), and each call substitutes the argument paths it was given and compares owners ([`lifetimes.md`](lifetimes.md) §1.1).

```zane
car Car(...);
engine Engine();
car!setEngine(engine);     // → car.engine = engine; one block owns both: legal
do() {
    spare Engine();
    car!setEngine(spare);  // ILLEGAL: this block does not outlive car's
}
```

**The subject is always a borrow.** `this` — the first parameter, and only it ([`functions.md`](functions.md) §2.1) — is a borrow of the object the method was called on, settled or roaming, and mutable under `mut`. A method never moves `this`, never stores it, and never returns it as `&T`. Nothing is written on `this` to say so, because a subject has no other mode. A verb that hands out a guest into an object takes that object as an `&T` parameter instead:

```zane
&Weapon weaponOf(player &Player) => player.weapon  // legal: rooted in a guest parameter
&Weapon weapon(this Player) => this.weapon         // ILLEGAL: this is a borrow
```

This rule preserves uniform call syntax. The call site writes `inspect(e)`, `setSpare(e)`, or `setEngine(e)` identically; only the callee's signature says which mode applies and therefore what the caller must supply and what state the caller is left in.

> **Story:** [`stories/memory.md`](../stories/memory.md#three-ways-to-hand-over-an-object) — "Three ways to hand over an object".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-borrow-comes-back-without-a-sigil) — "The borrow comes back, without a sigil".

### 2.10 Value-downstream enforcement (transitive value-only field restriction)

Value types form a closed world of plain value storage. A value-type field may contain only value types, value-type storage primitives among them (see [`syntax.md`](syntax.md) §2.6), and it **MUST NOT** contain a reference type (a `#`-marked type or a reference-type intrinsic such as `@primitives$List<T>`) or an `&`. This rule applies transitively: a value type containing another value type that eventually contains a reference-type or `&` field is also illegal.

Here, **downstream** means "through nested value-type fields." The restriction is checked recursively through the full value graph.

The rule is about **copying**, and both banned field kinds fail it the same way. An existing value is copied whole whenever a place expression is bound into a different slot (§2.3). A reference type is the opposite by construction: it exists in order *not* to be copied. It has exactly one host at a time (§2.1), an identity that guests name (§4), and it reaches a new place by being **moved** rather than duplicated (see [`lifetimes.md`](lifetimes.md) §1.2). Copying a value that contained one would have to do one of two things, and both dissolve that:

- **Duplicate the object**, minting a second instance with its own identity. Guests to the original would not follow the copy, and "exactly one host" would describe nothing.
- **Share the object**, so two values reach one host. Hosting would no longer be single, and a value would have become a way to alias.

An `&` field fails on the second directly: copying it would duplicate a guest inside a copied value, putting aliasing inside the one world that is defined by having none. `List` is a reference type and is covered by this restriction. `String` and `@primitives$String` are value types with owned backing stores; variable payload size does not itself imply reference semantics ([`types.md`](types.md) §2.7).

What this closure does **not** bar is **recursion**, and the reason is that a **boxed member** (§3.3) is not a reference-type field: it is out-of-line placement of the member's own declared type, and placement is not language-visible (§3.5). Nothing this rule forbids has entered the value. The recursion rule itself lives in [`adt.md`](adt.md) §4, and the copy that keeps such a value alias-free in §2.3.

```zane
type Vec2 = struct {
    x Float;
    y Float;
}

type Rect = struct {
    pos Vec2;
    size Vec2;
}

type Countdown = variant {
    done Unit;
    more Countdown;        // legal: a boxed member, deep-copied with the value
}

type BadOwner = struct {
    engine Engine;      // ILLEGAL: reference-type field inside a value type
}

type BadRef = struct {
    target &Engine;  // ILLEGAL: `&` field inside a value type
}
```

Downstream enforcement keeps hosting and guest bookkeeping confined to reference types, and — because nothing reachable from a value can be aliased, whether it is stored inline, in a backing store, or behind a box — is what lets a value be shared by snapshot and mutated concurrently under [`concurrency.md`](concurrency.md) §4.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".
> **Story:** [`stories/memory.md`](../stories/memory.md#what-a-copy-is-for-and-the-ban-that-survived-it) — "What a copy is for, and the ban that survived it".

### 2.11 Symbols require direct initialization

Every symbol declaration **MUST** provide its initial value in the declaration itself. Zane does not permit bare symbol declarations followed by conditional or delayed first assignment.

```zane
text String;  // ILLEGAL: symbols require direct initialization
```

```zane
text String("");  // LEGAL: directly initialized
if(runtimeBool()) {
    text = String("hi");
}
```

---

## 3. Memory Layout

### 3.1 Scope arenas and segmented offsets

Each lexical scope owns an **arena** made from two independent allocation regions:

- The **fixed-size region** stores materialized value-type slots, statically sized reference-type hosts, and the fixed-size handles — of dynamically-sized types of either kind and of boxed members alike — that are materialized in **scope-level** slots.
- The **dynamic region** stores the payloads behind those handles: the resizable backing stores of types such as `List` and `String`, and the payloads of boxed members (§3.6). A handle that sits *inside* a dynamic payload rather than in a scope slot — a boxed node's own boxed members, an element's owned storage — is part of that payload's block and is not separately placed.

Each region is a separate chain of fixed-size **1 MiB chunks** mapped from the OS on demand. A chunk belongs to exactly one region: fixed-size slots and dynamic backing stores never coexist in the same chunk. A region maps no chunk until its first allocation. When its current chunk cannot satisfy an allocation, the runtime maps another chunk for that region, assigns it the next **chunk id**, and makes it current.

Scopes nest last-in-first-out, and their arenas nest with them: both regions of a scope are unmapped in full the moment the scope drains (§3.2, [`lifetimes.md`](lifetimes.md) §2.1). Arena granularity is an implementation choice, like boolean packing (§3.4) and placement (§3.5) — the compiler may fold several lexical scopes into one arena. What the language fixes is the observable behavior: a scope's memory is released together when that scope drains, and no guest ever resolves into released memory. A value that escapes is promoted out of the draining scope first (§3.5); only a roaming host or a value escapes, and nothing guests either.

```text
one scope arena
──────────────────────────────
fixed-size region   dynamic region
[F1] → [F2]         [D1] → [D2]
```

An ordinary dynamic allocation never straddles a chunk boundary. A dynamic block of at most 1 MiB is wholly contained in one dynamic chunk; if the remaining bytes in the current chunk cannot hold it, allocation continues in a fresh dynamic chunk.

A dynamic block larger than 1 MiB is an **oversized span**: a dedicated contiguous OS mapping made from `ceil(block_size / 1 MiB)` consecutive dynamic chunks, all belonging exclusively to that block and assigned consecutive chunk ids. Its handle stores the segmented offset of the span's first byte and its exact block size, alongside the alignment that block was allocated at (§3.2). After resolving that base, element addressing uses an ordinary byte offset across the contiguous mapping. Every constituent chunk also has a directory entry. Returning an oversized span pushes only its base offset onto the exact-size stack; the complete span remains mapped for reuse until the scope drains.

Every chunk draws its id from one chunk directory, so payload locations, dynamic handles, guests, and size-stack entries all use one **`u32` segmented offset**:

```text
   u32 segmented offset
  ┌───────────────┬──────────────────────────┐
  │   chunk id    │   in-chunk word offset   │
  │  (high bits)  │       (low bits)         │
  └───────────────┴──────────────────────────┘
```

Allocations are at least 8-byte aligned, so the low bits count 8-byte words: a 1 MiB chunk holds 2¹⁷ words, so **17 low bits** address any slot in a chunk and the remaining **15 high bits** select one of up to 32768 live chunks — a reach of 32 GiB. The chunk directory maps a chunk id to the chunk's native base address, so an address is materialized only at use, as `directory[chunk id] + word offset × 8`: splitting the `u32` is a shift and a mask, and the directory lookup is one load.

Guests (§4.1), dynamic handles, and size-stack entries (§3.2) use segmented offsets. A payload may occupy segmented offset `0`, so a region's first allocation sits at a chunk base; no offset is reserved, because a guest is always initialized to a host (§2.11).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 3.2 Allocation, reuse, and teardown

The fixed-size region is a pure bump allocator: no size classes, no free list, no coalescing. A host has a fixed-size storage slot, so an overwrite consumes no new space in that region. Reference-type overwrite and move ordering follow §2.2 and §3.5. A materialized **value** slot follows the replacement rule of §2.3: the right-hand side observes the pre-overwrite occupant, and any overlapping replacement is completed before the old value ends. Only then are the old value's owned dynamic blocks — backing stores and boxed payloads alike, recursively — returned to their exact-size stacks and the replacement installed in the same slot. If the compiler proves the replacement does not depend on the current occupant, it may destroy the old value and construct a non-place result directly in that slot. Nothing in the fixed-size region is reclaimed individually — bytes in a slot that cease to be live before the scope drains remain dead space until teardown.

The dynamic region adds exact-size reuse on top of its bump frontier. Each scope maintains one LIFO **size stack** for every (byte size, alignment) pair that has become reusable. To allocate a dynamic block of size `S` and alignment `A`, the runtime first pops `size_stack[S, A]`; only when that stack is empty does it bump the dynamic frontier, rounding it up to `A` first. Keying on alignment as well as size is what keeps reuse sound now that blocks no longer share one alignment: a block returned by a type needing 8-byte alignment must not be handed to a type needing 16. It never satisfies a request from another size stack and never coalesces neighbouring blocks.

A block's size comes from what it holds, and the two kinds ask for different things. A **growable backing store** uses power-of-two byte sizes beginning at **128 bytes**, because that is where its doubling starts (§3.6); those sizes are a consequence of growth, not a classification imposed on the region. A **boxed payload** (§3.3) never grows, so it requests exactly the size of the one instance it holds — value type or reference type alike — and is aligned to that type's alignment requirement. There is no size class to round up to and no floor: a twelve-byte node occupies twelve bytes.

Returning a dynamic block pushes its base segmented offset onto the stack for its own size and alignment. The stacks are shared by all dynamic payloads in the scope, whatever produced them: a 128-byte block previously used by a `List<Int64>` may later hold string bytes, another list's elements, or a boxed node that happens to match it on both keys. Reuse is therefore exact and never approximate — a freed block serves only a request for the same number of bytes. This suits boxed payloads particularly well, because every instance of one type is the same size (a sum is laid out at its widest case plus tag), so the block a destroyed node returns is precisely what the next node of that type needs. An oversized span participates in the same exact-size policy.

When a scope drains — after all its spawned work completes ([`concurrency.md`](concurrency.md) §4.1) — the runtime unmaps its fixed-size and dynamic chunks in bulk, with no per-object teardown pass threaded through the exit. Logical destruction timing is independent of this: a value dies when its host, container, or scope does ([`lifetimes.md`](lifetimes.md) §2.1); it is the *memory* that is reclaimed together at drain.

> **Story:** [`stories/memory.md`](../stories/memory.md#when-the-free-stacks-fragment-and-the-arena-takes-the-scope) — "When the free stacks fragment, and the arena takes the scope".

### 3.3 Value and reference layout follow declaration order

Fields are laid out in declaration order. A value-type instance is stored inline, except for owned backing stores (§3.6) and any members the compiler boxes (below). A statically sized reference-type instance is also stored inline in a fixed-size host slot, so value-type slots and reference-type host slots may sit directly beside each other in the fixed-size region. Reference types differ by identity and hosting semantics, not by requiring a separate indirect allocation.

A reference-type instance carries no metadata of its own: a guest names it by its address (§4.1). A dynamically-sized reference type such as `List` occupies a fixed-size handle inline in the same region; only the backing store named by that handle occupies the dynamic region (§3.6).

A **boxed member** is laid out the same way: a fixed-size handle inline, with the instance it names placed in the dynamic region (§3.6). Which members are boxed is [`adt.md`](adt.md) §4's rule — required on a cycle of owning edges, where no finite inline layout exists, and permitted elsewhere per type. §3.5 makes the choice unobservable.

Boxing is available on **both** sides of the `#` axis. Two separate questions decide what a boxed member means, and they are answered by **different** types:

- **What the payload is** follows the **member's own declared type**, never the enclosing one. A reference-typed payload is an ordinary reference-type instance with identity: a boxed field takes its root's state as any field does (§2.8.1), and a boxed variant payload is roaming like any payload — being boxed neither grants nor withholds a guest. A value-typed payload is an ordinary value: no identity and nothing to guest. Because boxing is permitted off a cycle (§3.3, [`adt.md`](adt.md) §4), a reference type may box a value-typed member; that stores a plain value out of line and does **not** give it identity.
- **What becomes of the payload when the enclosing instance moves, is copied, or dies** follows the **enclosing type's kind**. A reference type *hosts* what it boxes: it destroys the payload when it dies, and moving it while it roams carries the payload with it (§3.5). A value type *owns* what it boxes: the payload is copied into fresh storage whenever the value is copied (§2.3) and returned when the value dies (§3.2).

Either way the member's declared type is unchanged by being boxed, and the box is placement rather than an extra level of type.

### 3.4 Booleans may be packed

The compiler may pack booleans in structs and arena frames when doing so does not change language semantics.

### 3.5 Statically sized storage uses the fixed-size region

Placement is an implementation decision, not a language-visible property. The arena model places every materialized, statically sized scope slot — value-type storage, a reference-type host, a dynamic type's fixed-size handle, or a boxed member's handle — inline in that scope's fixed-size region. The compiler may keep an unobservable value in registers or otherwise optimize its physical placement, but reference types do not require a separate heap allocation merely because they carry identity. A recursive member is boxed for the opposite reason: not because of which side of the `#` axis its type sits on, but because a finite inline layout does not exist for it (§3.3).

A move transfers a roaming host into a destination host of the **same type** ([`lifetimes.md`](lifetimes.md) §1). Both have the same statically known size, so a move copies the host's inline bytes, its handles among them, into the destination's fixed-size slot. A destination that already holds an object follows §2.2: a settled destination is overwritten in place, and any other destination's occupant is destroyed first. The source slot is spent ([`lifetimes.md`](lifetimes.md) §1.6). Nothing inside a roaming host is guested, so a move updates nothing else.

The **dynamic blocks** the host owns — the backing store behind a `List` or `String` handle, and the payload of a boxed member — stay where they are. A move copies their handles and never their contents, while the scope that holds the blocks outlives the destination host. A move whose destination outlives that scope is an **escape**: a `return` out of the scope that allocated the blocks, or a store into a host declared above it. Before that scope drains, every block the escaping host owns **MUST** reside in a scope that lives as long as the destination, so that no handle ever names released memory. The implementation either relocates each block — allocating an equal-size block or oversized span in such a scope's dynamic region, moving the live contents into it under their ordinary move rules, updating the handle, and returning the old block to its exact-size stack — or allocates the block there in the first place.

Relocation is **recursive**, because a relocated block may itself own dynamic blocks: a boxed payload holds its own boxed members, and a backing store holds its elements' owned storage. Relocating the root of a roaming recursive structure therefore relocates the whole structure, at a cost proportional to the number of boxed nodes it contains rather than to the root alone.

When a **value itself** is copied into a slot owned by another scope, it reaches that scope by copying rather than moving, and the same recursion applies to its blocks: the copy allocates each backing store and boxed payload afresh in the destination scope's dynamic region and copies into it, so the copy owns storage in the scope that holds it and the source keeps its own (§2.3). This governs the value being copied, not every value-typed payload in sight: one that a reference host owns through a boxed member travels with that host under the relocation rule above, because what becomes of a boxed payload follows the enclosing type's kind (§3.3).

Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and a guest names a settled host, which no placement decision moves once it has settled (§4).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-region-takes-the-boxes-and-a-box-asks-for-what-it-is) — "The region takes the boxes, and a box asks for what it is".
> **Story:** [`stories/memory.md`](../stories/memory.md#settled-overwrites-stay-in-place-and-only-an-escape-relocates) — "Settled overwrites stay in place, and only an escape relocates".

### 3.6 A handle has a fixed footprint; its payload lives in the dynamic region

Dynamically-sized types such as the reference type `List` and the value type `String` are represented as fixed-size **handles**. A handle records the payload's segmented offset and the metadata needed by the type, such as length and block size. The handle occupies a statically known footprint inline in the fixed-size region; its resizable backing store is a separate allocation in the dynamic region.

A type that contains a handle-typed field therefore stays statically sized:

```zane
type Inventory = #struct {
    items List<Item>;   // fixed-size handle inline; elements in the dynamic region
    count Int;
}
```

Dynamic block sizes are byte-based rather than element-type-based. A new list starts with a **128-byte block** — equivalent to sixteen 64-bit words — regardless of `T`. Its element capacity is `floor(block_bytes / stride(T))`. If one element does not fit in 128 bytes, the initial block is the smallest power-of-two block that can hold one element. Keeping list sizes to common byte values allows blocks to be reused across lists with different element types and across other dynamically-sized types; a boxed payload joins that reuse whenever its exact size and alignment happen to match a freed block.

A list grows according to the following rules:

1. When its capacity is exhausted, the requested block size is exactly twice its current block size.
2. The allocator first checks the size stack for that doubled size. If a block or oversized span is available, it is popped and the live elements are relocated into it.
3. If that stack is empty, the current backing store is the dynamic frontier allocation, the doubled size is at most 1 MiB, and the additional bytes fit before the current chunk boundary, the frontier is bumped by the additional bytes and the store grows in place.
4. Otherwise, a doubled block of at most 1 MiB is bump-allocated wholly inside one dynamic chunk. A doubled block larger than 1 MiB is allocated as a fresh dedicated oversized span (§3.1). The live elements are relocated into the new block or span.
5. After relocation, the handle's backing-store offset and block size are updated and the old block's base offset is pushed onto the stack for its exact old byte size.

A block never grows in place across a chunk boundary, and an oversized span is never extended in place: further growth relocates into a doubled oversized span after checking that exact-size stack first. Relocation moves or copies elements according to their type's ordinary move rules; the old block becomes reusable only after its previous occupants are no longer live. Guests to the list remain valid because they reach the list's host, whose fixed-size handle now names the current backing store.

A **boxed member** (§3.3) uses the same two-part representation with a payload that never grows. Its handle records the payload's segmented offset; the payload is one instance of the member's declared type, sized and aligned as §3.2 specifies, and is returned to its size stack when the member's enclosing instance is destroyed. Overwriting the member writes the replacement into the same block, which always fits because both are instances of the member's type, and the replacement's own boxed members are written into the blocks the occupant already holds, recursively (§2.2). A guest into a settled boxed member, or into any member below it, keeps its address. A payload larger than 1 MiB is a dedicated oversized span like any other. None of the growth rules above apply to it: a boxed payload is allocated once and is thereafter only relocated when its roaming host escapes, or allocated afresh by a deep value copy (§2.3, §3.5).

Dynamic chunks and oversized spans begin at cache-line-aligned addresses, and a **growable backing store** — 128 bytes or larger — is cache-line aligned within them. Every other block takes its own type's alignment, which §3.2 applies to reuse and to the frontier alike, so frontier allocations, reused blocks, and dedicated spans all keep their alignment without mixing payloads into fixed-size chunks.

> **Story:** [`stories/memory.md`](../stories/memory.md#a-free-zero-sentinel-and-cache-line-aligned-buffers) — "A free zero sentinel, and cache-line-aligned buffers".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-region-takes-the-boxes-and-a-box-asks-for-what-it-is) — "The region takes the boxes, and a box asks for what it is".

---

## 4. Guests

### 4.1 A guest is a settled host's segmented offset

A guest stores the **`u32` segmented offset** (§3.1) of the settled host it names. At half the width of a 64-bit pointer, twice as many guests fit in a cache line, and resolving one is the chunk-directory load every segmented offset needs. An explicitly declared `&T` slot contains only this offset.

A guest minted from a field path such as `car.engine` stores the offset of `engine` inside `car`. A guest copied from another guest copies its offset (§2.6). Nothing is allocated to mint a guest, and nothing is recorded in the host.

```zane
dps Float = mainWeapon.dps;  // mainWeapon's offset → directory → host → dps
```

> **Story:** [`stories/memory.md`](../stories/memory.md#guests-without-anchors) — "Guests without anchors".

### 4.2 Why a guest never dangles

A guest could dangle only if the host it names moved or died while the guest lived. A settled host never moves (§2.1): it is overwritten in place (§2.2), including a boxed member (§3.6), and nothing inside dynamic storage is guested (§2.8.1). It dies when its scope drains, and the store rule ensures every guest to it is owned by that scope or a nested one ([`lifetimes.md`](lifetimes.md) §1.1), so the guest dies no later. A scope with spawned work drains only after that work finishes ([`concurrency.md`](concurrency.md) §4.1).

An overwrite destroys the old occupant while guests to the slot remain. They name the slot, not the occupant, and observe the replacement (§2.2). This is the only change of object a guest ever sees.

> **Story:** [`stories/memory.md`](../stories/memory.md#guests-without-anchors) — "Guests without anchors".

---

## 5. Language Comparisons

### 5.1 Hosting and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single host by default | ✅ | ❌ | ❌ | ✅ |
| Non-hosting guests as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Reference counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Guested objects never move | ✅ settled hosts | ❌ | ❌ | ⚠️ only while borrowed |
| Host overwrite keeps existing guests valid | ✅ in place | ❌ | ❌ | ⚠️ heavily restricted by borrow checking |

### 5.2 Allocation

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Allocation strategy | per-scope fixed/dynamic arenas | runtime-managed | allocator-dependent | allocator-dependent |

> **See also:** [`lifetimes.md`](lifetimes.md) §3 for the lifetime and destruction behavior comparison.

---

## 6. Summary

| Concept | Rule |
|---|---|
| Settled host | May be guested; never moves; a bare reference-type symbol, or a field of a settled root |
| Roaming host | May move; nothing guests it or anything inside it; written `^T`, and every list element and variant payload |
| Settling | A roaming host settles by moving into a settled place; a settled host never roams again |
| Hosting storage | Reference-typed symbols, fields, and container elements are directly initialized and may later be overwritten |
| Settled overwrite | Destroys the old occupant and writes the replacement at the same address, reusing the block of every boxed member reached through struct fields and `ArrayRef` elements; guests to the slot or any of its fields observe the replacement |
| Value type | Mutable in place through a borrowed `mut` subject; storage may also be overwritten freely |
| Value construction | A non-place value expression constructs directly in its eventual destination, recursively through nested fresh results; only an existing place is copied |
| Value overwrite | The right-hand side observes the pre-overwrite value; an overlapping replacement is completed before the old value and its owned blocks are destroyed |
| Value copy | Copies the whole existing value: inline bytes, plus a fresh allocation and recursive copy of every backing store and boxed payload the value owns, so two values never share storage; a copy from a place never read again may be a move |
| `&` (guest) | Guest-only non-hosting storage naming a settled host; may be repointed, copied by value, and returned, but can never host a `T` |
| Spent host slot | After a roaming host's value moves out, the slot is spent and keeps enough storage for a store to refill it |
| Place expression | Existing storage: a named symbol, a field access of a place, a place-projection subscript of a place, or an `&` parameter |
| New `&` value | May be minted only from a settled place: a bare settled symbol, or a path from a settled root or an `&T` parameter through struct fields and `ArrayRef` elements only; roaming hosts, list elements, variant payloads, and temporaries are rejected |
| `ArrayRef` element | Fixed storage: takes its root's state, may be guested under a settled root, is overwritten in place, and is never moved out |
| Field of a roaming root | May be moved out; the root is partly spent until refilled; a field is never declared roaming |
| Borrow | Non-hosting, non-escaping access to a caller's storage for the duration of a call; not storable, not returnable, not a guest source, not a move-source |
| Value-type parameter | Always a read-only borrow; copied only when the parameter is itself bound into a fresh slot |
| Reference-type parameter | `T` borrows; `^T` takes a roaming host or temporary, spending the caller's symbol; `&T` takes a guest and leaves the caller a full host |
| Subject | Always a borrow, mutable under `mut`; never moved, stored, or returned as `&T` |
| Value-downstream enforcement | Value types may contain only value types, value-type primitives among them, transitively — never a reference-type or `&` field, because a reference type is made to be moved rather than copied; recursion is **not** barred, since a boxed member is placement rather than a reference-type field |
| `&` targets reference types | An `&T` requires `T` to be a reference type; a value is shared by copy or borrow, never by a stored `&` |
| Symbol declaration | Must be directly initialized |
| Reference-type placement | Inline storage is bump-allocated in the creating scope's fixed-size region; moving a roaming host copies its inline bytes and handles, and its dynamic blocks stay put unless it escapes the scope holding them, which must first leave every block in a scope that lives as long as the destination |
| Boxed member | A member whose type can lead back to the enclosing type is stored as a fixed-size handle inline with its enclosing instance, while the instance the handle names lives in the dynamic region; required on a containment cycle, permitted elsewhere, and nothing marks it in the source. In a reference type it is a **hosting** member; in a value type the value owns it outright and deep-copies it. An overwrite reuses its block |
| `&` representation | A guest is the `u32` segmented offset of the settled host it names |
| Addressing | Every chunk shares one `u32` segmented-offset directory; 8-byte-aligned offsets reach 32 GiB across up to 32768 1 MiB chunks |
| Dynamic allocation | Exact-size stack first, frontier second; a growable backing store uses power-of-two sizes from 128 bytes because it doubles, while a boxed payload asks for exactly its type's size and has no class; blocks above 1 MiB use dedicated contiguous oversized spans |
| Dynamic-block alignment | A growable backing store is cache-line aligned; a boxed payload takes its type's alignment; the frontier is rounded up before it is bumped (§3.6) |

> **See also:** [`lifetimes.md`](lifetimes.md) §4 for the summary of scope, move, and destruction rules.
