# Zane Memory Model

This document specifies Zane's memory model: hosting, guests, anchors, and arena layout. Lexical lifetime rules, rehosting, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for value and reference types. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling guests by combining single hosting, lexical lifetime rules, and anchor-based tracking.

- **`Overwritable hosts`.** A reference-type host is directly initialized and may later be overwritten.
- **`Guests ride on reference types`.** An `&` — a **guest** — is a non-hosting handle to a **reference type** (a `#`-marked type); a value type has no identity to anchor, so it is shared by copy or scoped borrow, never by a stored guest.
- **`Bare symbols are not guest sources`.** A new guest may be minted only from a field access or from an `&T` parameter — never from a bare symbol (§2.8). A local's own hosting slot is therefore never the thing a guest points at.
- **`Three passing modes`.** A reference-type parameter is written `T` to **swallow** it, `&T` to take a **guest**, or `'T` to **borrow** it for the call (§2.9).
- **`Repointable guests`.** A guest is non-hosting storage that can point at different hosts over time.
- **`Lexical lifetime enforcement`.** Guest assignment and rehosting are checked using declaration scope alone (see [`lifetimes.md`](lifetimes.md) §1).
- **`Deterministic destruction`.** Objects are destroyed when their hosting scope drains; there is no tracing garbage collector (see [`lifetimes.md`](lifetimes.md) §2).
- **`Regioned arena placement`.** Every scope owns separate fixed-size and dynamic regions. Statically sized storage is placed inline in the fixed-size region; resizable data and the boxed payloads of recursive hosting fields use the dynamic region. Anchors live outside scope arenas in one runtime-global fixed-slot pool (see §3 and §4).
- **`Segmented-offset tethers`.** Internally, each guest is represented by a `u32` tether — a chunk id plus an in-chunk offset — that points at an anchor cell in the host's identity path, not a raw pointer (see §4.2).

The source language and runtime use separate terms: an object lives in a **host**, and a **guest** (`&T`) may access it without storing it or controlling its lifetime. Internally, each guest is represented by a **tether** that resolves through an **anchor**. Moving the object updates its terminal anchor or links an older anchor to the destination anchor, so existing tethers — and therefore guests — continue to reach it.

These rules fit together mechanically. Hosts are the only storage that controls destruction. A guest may be minted only from a field or an `&` parameter — never from a temporary, and never from a bare symbol. Lexical scope checks ensure the host outlives every guest derived from it. When an object is rehosted or a host is overwritten, guests stay valid. Internally, their tethers follow the host's anchor rather than a fixed object address.

> **Story:** [`stories/memory.md`](../stories/memory.md#safety-without-a-collector-and-without-lifetimes) — "Safety without a collector and without lifetimes".

---

## 2. Hosting and Storage

### 2.1 Every reference-type instance has exactly one host

Every instance of a reference type (a `#`-marked type, see [`types.md`](types.md) §2.1) is hosted by exactly one symbol, field, or container slot at a time. Hosting is the default storage mode for reference values.

### 2.2 Reference-type hosts are overwritable after initialization

Any hosting storage position for a reference-type instance—a symbol, field, or container slot—**MUST** be directly initialized, and **MAY** later be overwritten.

```zane
tank Tank(...)
tank = Tank(...) // legal
```

Overwriting a host does not invalidate existing guests. Guests follow the host/anchor path, so later reads observe the host's current value.

Container overwrite therefore does not depend on whether the element slot stores a host or a guest. Both kinds of slots may be rewritten after initialization.

```zane
hosts Array<Node, 2> = [Node(), Node()]
```

Rewriting `hosts[1]` replaces the hosted reference-type instance in that slot. Guests to that slot observe the new value because guests follow the host/anchor path, not the original object.

### 2.3 Value types are mutable in place and freely overwritable

Value types have no anchor and no heap identity. A value is mutated in place through a `mut` method whose `this` is a borrow of the value's storage (see [`effects.md`](effects.md) §2.3, [`functions.md`](functions.md) §2.4), and its storage slot may also be reassigned wholesale. Neither operation goes through the anchor system, because a value has no identity to track.

```zane
pos Vec2(1, 2)
pos!setX(Float(3)) // in-place field write through a borrow of pos
pos = Vec2(3, 4)   // whole-slot overwrite
```

### 2.4 `&` is a guest: non-hosting storage

`&` creates a **guest**: non-hosting storage that points at a **reference type** only. An `&T` requires `T` to be a reference type — a declared `#struct`/`#variant`/`#enum` — because only a reference type carries the identity (the anchor, §4) that a stable, move-surviving guest needs. A value type is shared by copying it or by a scoped borrow (see [`functions.md`](functions.md) §2.4), never by a stored guest. Writing `&Node` names a guest to a reference type; a bare `&Int` over a value type is ill-formed.

An explicitly declared `&T` slot is **guest-only**: it stores only a tether and can never directly host a `T`. A slot declared as `T` is **host-capable**. After its value is rehosted, that same full-size slot may remain readable in guest state, but it retains the storage needed to host another `T` later. Guest-only and host-capable guest states use the same access semantics, but only the latter can become a host again.

A guest may be declared as:

- a local symbol
- a reference-type field
- an element type inside another storage type
- a function or constructor parameter
- a function return type

An `&` type is legal in storage sites (local symbols, fields, nested storage types), function parameter positions, and function return-type positions. The borrow type `'T` (§2.9) is legal in parameter positions only: a borrow is not storage and never escapes its call.

Declaring an `&` symbol is legal, but the restriction in §2.8 governs what may initialize it: a guest is minted from a field or from an `&T` parameter, not from a bare symbol.

> **Story:** [`stories/memory.md`](../stories/memory.md#two-vocabularies-host-and-guest-above-anchor-and-tether) — "Two vocabularies: host and guest above anchor and tether".

### 2.5 Guests are repointable

An `&` symbol or `&` field may be assigned a different target later, as long as the new target is a guest source (§2.8) and the scope rule in [`lifetimes.md`](lifetimes.md) §1.1 is satisfied.

### 2.6 Guests are independent

Assigning or passing a guest gives the destination its own guest to the same host. The runtime may resolve a forwarding tether and store the terminal anchor identity in the new guest; this canonicalization is unobservable. Rebinding one guest's storage site later changes only that storage site; it does not retarget other guests that already point to that host.

### 2.7 Guests and hosts use the same surface operations

At use sites, a guest is used with the same surface syntax as a direct host. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between host and guest matters only at the storage site: a guest stores a non-hosting link, while a host stores the object itself or its hosting slot.

### 2.8 Place expressions and new `&` values

A **place expression** is an expression that denotes an existing, stable storage location.

The following are place expressions:

- a named local, field-backed, or hosting/`&` storage symbol such as `engine`
- a field access whose base is a place, such as `car.engine` or `this.engine`
- a subscript expression `list[index]` when `list` is a place expression and `[]` is defined as a place projection for that subject type
- an `&T` guest parameter or a `'T` borrow parameter inside the callee body (§2.9)

Only some place expressions may mint a new guest. A new `&` value may be minted from:

- a field access whose base is a place **and whose base chain does not pass through a `'T` borrow parameter**, such as `car.engine` or `this.engine` on a guest subject
- an `&T` parameter

Everything else is rejected. In particular:

- A **bare symbol** is never a guest source, even though it is a place expression (§2.8.1).
- A `[]` expression is never a guest source, even though it is a place expression.
- A field access rooted in a `'T` borrow parameter is never a guest source. A borrow does not escape its call (§2.9), and it would escape just as surely inside a guest minted from one of its fields as it would on its own.
- Temporaries and other value-only expressions are not place expressions at all. Constructor calls and ordinary function results such as `Engine()` and `makeEngine()` are not places.

```zane
engine &Engine = Engine()   // ILLEGAL: Engine() is a temporary, not a place expression
```

```zane
car Car()
r &Engine = car.engine   // legal: field access on a place
```

```zane
armory Armory()
weapons List<&Weapon> = [armory.primary, armory.backup]
current &Weapon = weapons[1]   // legal: reads an `&Weapon` already stored in the list
```

The last line works because `weapons[1]` reads an `&Weapon` value the list already holds. It does not mint a new `&` from a hosting element. Those stored guests are stable because the language does not let `[]` mint guests from host storage in the first place.

Non-`&` host bindings may be initialized from any expression, including temporaries. The host materializes the value into stable storage.

```zane
engine Engine()         // legal: plain host binding; Engine() temporary is materialized into engine
```

### 2.8.1 A bare symbol is not a guest source

A **bare symbol** — an identifier naming a local, a parameter, or a package constant, standing alone rather than as the base of a field access — **MUST NOT** be used to mint a new `&`.

```zane
engine Engine()
r &Engine = engine        // ILLEGAL: a bare symbol is not a guest source
inspect(engine)           // ILLEGAL if inspect takes `&Engine`
```

The reason is that a bare symbol's hosting slot is exactly the storage the language lets you overwrite most freely (§2.2, [`lifetimes.md`](lifetimes.md) §1). Without this rule a program can write:

```zane
main Player()
second Player()
guest &Player = main      // ILLEGAL under this rule
second = main
```

`second = main` moves the object out of `main`'s slot, and `main` downgrades to guest state ([`lifetimes.md`](lifetimes.md) §1.6). What `guest` should then denote — the object that left, or the slot it left from — has no answer that is right in both directions, and every candidate answer costs either a rule the programmer has to carry or machinery the runtime has to pay for. Removing the source removes the question: line 3 is a compile-time error, so no guest ever depends on a bare symbol's slot.

Nothing is lost by it. A guest exists so that storage which does not own an object can still reach it — an `&` field, a container element, an `&T` parameter inside a callee. A bare symbol is *already* in scope wherever a guest to it could be declared, so the guest never buys reach that the symbol itself did not already have. What a bare symbol is genuinely needed for is passing an object into a call. It may still be swallowed by a plain `T` parameter, which takes hosting outright; where the call must *not* take hosting, the borrow mode `'T` is what carries it (§2.9), reading and mutating the caller's object for the duration of the call without minting a guest to it.

A **field** is a different matter and stays a legal source. A field belongs to an object whose own lifetime the host system already tracks, and a guest to `car.engine` follows that field's host through the anchor path (§4.5) when the field is overwritten or the containing object is rehosted. This is what makes the restriction narrow: it constrains where guests come from, not what they can survive.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-new-ref-may-come-from) — "Where a new ref may come from".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-slot-that-could-not-be-pointed-at) — "The slot that could not be pointed at".

### 2.9 Function parameters: swallow, guest, and borrow

A **borrow** is non-hosting, non-escaping access to a caller's storage for the duration of a call. A borrow is never itself storage: unlike a guest (§2.4) it has no anchor, and it **MUST NOT** be stored in a field or returned. It exists only while the call runs.

That restriction is on the borrow, not on what is read through one. A value type is *always* passed this way — a value-type parameter is a **read-only borrow** of the caller's slot — and binding through that borrow into a fresh slot (an assignment, a new declaration, or a field or return store) **copies** the value. The copy is a new value that outlives the call perfectly well; what does not escape is the borrow. A reference type has no such copy, so a `'T` borrow leaves nothing behind at all.

A **reference type** parameter has three passing modes, one per surface form. The subject parameter `this` is not one of these positions and has its own rule, below:

| Mode | Written | Caller supplies | The callee may |
|---|---|---|---|
| Swallow | `T` | a move-source ([`lifetimes.md`](lifetimes.md) §1.2) | take hosting access; the caller's symbol downgrades to a guest |
| Guest | `&T` | a guest source (§2.8) | store it in `&` storage or return it as `&T` |
| Borrow | `'T` | any place expression, **including a bare symbol** | read and mutate it for the duration of the call only |

- A parameter declared as a plain reference type `T` **swallows** its argument — it takes the value by hosting access. The value belongs to the call-site scope, not the callee body ([`lifetimes.md`](lifetimes.md) §1.5), so it outlives the call. Passing a hosting value to such a parameter downgrades the caller's symbol to a guest ([`lifetimes.md`](lifetimes.md) §1.8), whatever the callee does with it — whether the verb relays the host back through its return or consumes it outright.
- A parameter declared as `&T` is a **guest**: the caller supplies a source that may mint a new guest under §2.8 (so `T` is a reference type, §2.4), and inside the callee body it acts as a place expression that may be stored into `&` storage or returned as `&T` under [`lifetimes.md`](lifetimes.md) §1.7. Because a bare symbol is not a guest source, an `&T` parameter can only be fed from a field, a container's stored guest, or another `&T` parameter.
- A parameter declared as `'T` is a **borrow**: the caller may supply any place expression, a bare symbol included, and the callee gets read and `mut` access for the call and nothing more. A `'T` parameter **MUST NOT** be stored in `&` storage, returned as `&T`, or used as a move-source, and neither may a field reached through it (§2.8); `'T` is not a legal storage, field, or return type. Passing a host to a `'T` parameter leaves the caller a full host: nothing downgrades.

`'T` is the mode that keeps ordinary calls ordinary. Under §2.8.1 a bare local cannot feed an `&T` parameter, so a verb that merely wants to read or mutate a caller's object declares that object `'T`:

```zane
Float topSpeed(engine 'Engine) => engine.speed

engine Engine()
s Float = topSpeed(engine)   // legal: a bare symbol may be borrowed
```

Passing a value by borrow is the semantic model for both worlds; where a read-only borrow is indistinguishable from a copy, the compiler may still pass a small value by copy, the same latitude placement has (§3.5). The distinction becomes observable under concurrent sharing, where a spawned reader sees the borrowed value live (see [`concurrency.md`](concurrency.md) §4.4).

```zane
type Car = #struct {
    engine &Engine;   // an `&` field
    spare Engine;     // a hosting field
    _value Int;
}

// `&` parameter is a guest; it may be stored into an `&` field
Unit setEngine(this Car, engine &Engine) mut {
    this.engine = engine
    return Unit()
}

// plain reference-type parameter: taken by hosting access, then moved into a hosting field of this
Unit setSpare(this Car, engine Engine) mut {
    this.spare = engine
    return Unit()
}

// borrow parameter: a reference-type object read without consuming it and without minting a guest
Int inspect(this Car, engine 'Engine) {
    return this._value + engine.speed
}
```

**The subject parameter is never a swallow position.** A method does not consume the object it is called on, so `this` — the first parameter, and only it ([`functions.md`](functions.md) §2.1) — chooses between two of the three modes rather than all three: it is a **borrow** written bare, or a **guest** written `this &T` when the method stores the subject past the call or returns it as `&T` (see [`functions.md`](functions.md) §2.4). `'` is **never** written on `this`.

So bare `T` does not mean the same thing in both positions — on an ordinary parameter it swallows, on `this` it borrows — because `this` was never a swallow position to begin with. That much predates the borrow mode: a bare reference-type `this` used to be an implicit **guest**, likewise never swallowed. What changed is only *which* non-swallowing mode it is, and it moved to the borrow because the subject expression at a call site is usually a bare symbol, which §2.8.1 no longer admits as a guest source. Value and reference subjects now agree: `this` carries at most one marker, `&`, and its absence means borrow.

Binding a swallowed or borrowed parameter into `&` storage is illegal. A swallowed value is hosted at the call site while an `&` field lives with the object that holds it — which may outlive the call. A borrow does not survive the call at all:

```zane
Unit setEngineSwallowed(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: a swallowed host is not a guest source
    return Unit()
}

Unit setEngineBorrowed(this Car, engine 'Engine) mut {
    this.engine = engine   // ILLEGAL: a borrow is not a guest source and does not escape the call
    return Unit()
}
```

This rule preserves uniform call syntax. The call site writes `consume(e)`, `inspect(e)`, or `setEngine(e)` identically; only the callee's signature says which mode applies and therefore what the caller must supply and what state the caller is left in.

> **Story:** [`stories/memory.md`](../stories/memory.md#three-ways-to-hand-over-an-object) — "Three ways to hand over an object".

### 2.10 Value-downstream enforcement (transitive value-only field restriction)

Value types form a closed world of plain value storage. A value-type field may contain primitives (see [`syntax.md`](syntax.md) §2.1) and other value types, but it **MUST NOT** contain a reference type (a `#`-marked type) or an `&`. This rule applies transitively: a value type containing another value type that eventually contains a reference-type or `&` field is also illegal. The same closure forbids a value type from recursing: a self-reference needs indirection, and both of Zane's indirections — the `&` guest and the boxed hosting member (§3.3, [`adt.md`](adt.md) §4) — belong to reference types alone.

Here, **downstream** means "through nested value-type fields." The restriction is checked recursively through the full value graph.

Value types are copied and overwritten as ordinary inline values. They do not have per-instance anchors or destruction tracking. If a value could contain a reference-type field, copying it would silently duplicate hosting. If a value could contain an `&`, copying it would silently duplicate non-hosting tracking state without going through the anchor system. Downstream enforcement keeps value copying mechanical, keeps hosting/guest bookkeeping confined to reference types, and — because nothing reachable from a value can be aliased — is what lets a value be shared by snapshot and mutated concurrently under [`concurrency.md`](concurrency.md) §4.

```zane
type Vec2 = struct {
    x Float;
    y Float;
}

type Rect = struct {
    pos Vec2;
    size Vec2;
}

type BadOwner = struct {
    engine Engine;      // ILLEGAL: reference-type field inside a value type
}

type BadRef = struct {
    target &Engine;  // ILLEGAL: `&` field inside a value type
}
```

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".

### 2.11 Symbols require direct initialization

Every symbol declaration **MUST** provide its initial value in the declaration itself. Zane does not permit bare symbol declarations followed by conditional or delayed first assignment.

```zane
text String   // ILLEGAL: symbols require direct initialization
```

```zane
text String = ""   // LEGAL: directly initialized
if runtimeBool() {
    text = "hi"
}
```

---

## 3. Memory Layout

### 3.1 Scope arenas, the global anchor pool, and segmented offsets

Each lexical scope owns an **arena** made from two independent allocation regions:

- The **fixed-size region** stores materialized value-type slots, statically sized reference-type hosts, and the fixed-size handles of dynamically-sized reference types.
- The **dynamic region** stores the payloads behind fixed-size handles: the resizable backing stores of types such as `List` and `String`, and the boxed payloads of recursive hosting fields (§3.6).

Each region is a separate chain of fixed-size **1 MiB chunks** mapped from the OS on demand. A chunk belongs to exactly one region: fixed-size slots and dynamic backing stores never coexist in the same chunk. A region maps no chunk until its first allocation. When its current chunk cannot satisfy an allocation, the runtime maps another chunk for that region, assigns it the next **chunk id**, and makes it current.

Scopes nest last-in-first-out, and their arenas nest with them: both regions of a scope are unmapped in full the moment the scope drains (§3.2, [`lifetimes.md`](lifetimes.md) §2.1). Arena granularity is an implementation choice, like boolean packing (§3.4) and placement (§3.5) — the compiler may fold several lexical scopes into one arena. What the language fixes is the observable behavior: a scope's memory is released together when that scope drains, and no guest ever resolves into released memory. A value that escapes is promoted out of the draining scope first (§3.5, §3.7), and its guests reach the promoted value through the terminal anchor path (§4.5).

Anchors do not belong to any scope arena. The runtime owns one **global anchor pool**, implemented as a lazy chain of anchor-only 1 MiB pages. Every anchor occupies an **8-byte-aligned, 8-byte physical slot**: the first four bytes hold a `u32` target segmented offset and the remaining four bytes identify whether that target is a hosted payload or another anchor. An anchor page therefore contains 131072 addressable slots. The pool maps its first page only when the program creates its first guest and adds another page only when its current frontier and free-address stack cannot satisfy an allocation.

```text
one scope arena                        runtime-global anchor pool
──────────────────────────────         ──────────────────────────
fixed-size region   dynamic region     [anchor page] → [anchor page] → ...
[F1] → [F2]         [D1] → [D2]
```

An ordinary dynamic allocation never straddles a chunk boundary. A dynamic block of at most 1 MiB is wholly contained in one dynamic chunk; if the remaining bytes in the current chunk cannot hold it, allocation continues in a fresh dynamic chunk.

A dynamic block larger than 1 MiB is an **oversized span**: a dedicated contiguous OS mapping made from `block_size / 1 MiB` consecutive dynamic chunks, all belonging exclusively to that block and assigned consecutive chunk ids. Its handle stores the segmented offset of the span's first byte and its size class. After resolving that base, element addressing uses an ordinary byte offset across the contiguous mapping. Every constituent chunk also has a directory entry. Returning an oversized span pushes only its base offset onto the exact-size stack; the complete span remains mapped for reuse until the scope drains.

Scope chunks and global anchor pages draw ids from the same chunk directory, so payload locations, dynamic handles, tethers, backpointers, anchor cells, and size-stack entries all use one **`u32` segmented offset**:

```text
   u32 segmented offset
  ┌───────────────┬──────────────────────────┐
  │   chunk id    │   in-chunk word offset   │
  │  (high bits)  │       (low bits)         │
  └───────────────┴──────────────────────────┘
```

Allocations are at least 8-byte aligned, so the low bits count 8-byte words: a 1 MiB chunk holds 2¹⁷ words, so **17 low bits** address any slot in a chunk and the remaining **15 high bits** select one of up to 32768 live chunks — a reach of 32 GiB. The chunk directory maps a chunk id to the chunk's native base address, so an address is materialized only at use, as `directory[chunk id] + word offset × 8`: splitting the `u32` is a shift and a mask, and the directory lookup is one load.

Tethers (§4.2), per-host backpointers (§4.2), anchor cells (§4.1), dynamic handles, and size-stack entries (§3.2) use segmented offsets. The value `0` is the *untethered* sentinel wherever an anchor identity is expected. The global anchor pool never issues `0` as an anchor identity: if its first page is assigned chunk id `0`, that page's first slot is left permanently unused. Payloads carry no such restriction and may occupy segmented offset `0`, so a region's first allocation sits at a chunk base.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 3.2 Allocation, reuse, and teardown

The fixed-size region is a pure bump allocator: no size classes, no free list, no coalescing. A host has a fixed-size storage slot, so overwriting it destroys the current occupant and initializes the replacement directly in the same slot (§2.2, §3.7); the overwrite consumes no new space in the fixed-size region. Any dynamic blocks owned by the destroyed occupant — backing stores and boxed payloads alike, recursively — are returned to their exact-size stacks before the replacement becomes live. Nothing in the fixed-size region is reclaimed individually — bytes in a slot that cease to be live before the scope drains remain dead space until teardown.

The dynamic region adds exact-size reuse on top of its bump frontier. Each scope maintains one LIFO **size stack** for every block size that has become reusable. To allocate a dynamic block of size `S`, the runtime first pops `size_stack[S]`; only when that stack is empty does it bump the dynamic frontier, rounding it up to the requested block's alignment first. It never satisfies a request from another size stack and never coalesces neighbouring blocks.

A block's size comes from what it holds, and the two kinds ask for different things. A **growable backing store** uses power-of-two byte sizes beginning at **128 bytes**, because that is where its doubling starts (§3.6); those sizes are a consequence of growth, not a classification imposed on the region. A **boxed payload** (§3.3) never grows, so it requests exactly the size of the one reference-type instance it holds and is aligned to that type's alignment requirement. There is no size class to round up to and no floor: a twelve-byte node occupies twelve bytes.

Returning a dynamic block pushes its base segmented offset onto the stack for that exact byte size. The stacks are keyed by byte size alone and are shared by all dynamic payloads in the scope, whatever produced them: a 128-byte block previously used by a `List<Int64>` may later hold string bytes, another list's elements, or a boxed node that happens to be 128 bytes wide. Reuse is therefore exact and never approximate — a freed block serves only a request for the same number of bytes. This suits boxed payloads particularly well, because every instance of one reference type is the same size (a `#variant` is laid out at its widest case plus tag), so the block a destroyed node returns is precisely what the next node of that type needs. An oversized span participates in the same exact-size policy.

The global anchor pool has one LIFO **free-address stack**, because every anchor slot has the same size. Creating an anchor pops that stack first; only when it is empty does allocation bump the global anchor frontier, mapping another anchor page as needed. Returning an anchor pushes its segmented offset onto the same stack. Anchor pages remain mapped and retain their chunk-directory entries until runtime shutdown, including when every slot on a page is free; consequently every offset retained by the stack always resolves to its original anchor slot and anchor chunk ids are never repurposed during the run.

When a scope drains — after all its spawned work completes ([`concurrency.md`](concurrency.md) §4.1) — the runtime unmaps its fixed-size and dynamic chunks in bulk, with no per-object teardown pass threaded through the exit. Logical destruction timing is independent of this: a value dies when its host, container, or scope does ([`lifetimes.md`](lifetimes.md) §2.1); it is the *memory* that is reclaimed together at drain. Global anchor pages are not tied to scope teardown: terminal payload anchors are returned when their hosting lineages end, while forwarding anchors are returned from the former source scope's retirement stack when that scope drains (§4.6). The pages themselves remain mapped until runtime shutdown.

> **Story:** [`stories/memory.md`](../stories/memory.md#when-the-free-stacks-fragment-and-the-arena-takes-the-scope) — "When the free stacks fragment, and the arena takes the scope".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 3.3 Value and reference layout follow declaration order

Fields are laid out in declaration order. Value types are stored inline. A statically sized reference-type instance is also stored inline in a fixed-size host slot, so value-type slots and reference-type host slots may sit directly beside each other in the fixed-size region. Reference types differ by identity and hosting semantics, not by requiring a separate indirect allocation.

A reference-type instance carries one `u32` backpointer field of anchor metadata (a segmented offset, §4.2) that remains `0` until the instance is first tethered. A dynamically-sized reference type such as `List` occupies a fixed-size handle inline in the same region; only the backing store named by that handle occupies the dynamic region (§3.6).

A **boxed hosting member** is laid out the same way. A hosting field whose declared type can lead back to the enclosing type through hosting fields would have no finite inline size, so the compiler stores it as a fixed-size handle inline and places the reference-type instance it hosts in the dynamic region (§3.6). Boxing is what keeps every type on such a containment cycle statically sized; which fields are boxed follows from the declarations alone (see [`adt.md`](adt.md) §4). A boxed instance is an ordinary reference-type instance in every other respect: it carries its own backpointer, is reached through its host's field, and may be guested from that field access under §2.8.

### 3.4 Booleans may be packed

The compiler may pack booleans in structs and arena frames when doing so does not change language semantics.

### 3.5 Statically sized storage uses the fixed-size region

Placement is an implementation decision, not a language-visible property. The arena model places every materialized, statically sized scope slot — value-type storage, a reference-type host, a dynamic type's fixed-size handle, or a boxed hosting member's handle — inline in that scope's fixed-size region. The compiler may keep an unobservable value in registers or otherwise optimize its physical placement, but reference types do not require a separate heap allocation merely because they carry identity. A recursive hosting field is boxed for the opposite reason: not because it is a reference type, but because a finite inline layout does not exist for it (§3.3).

When a reference-type instance is rehosted, all storage owned by that host is relocated into storage owned by the destination. Its statically sized inline bytes are copied into the destination host's fixed-size slot (§3.7). For every **dynamic block** the host owns — a resizable backing store behind a `List` or `String` handle, or the boxed payload of a recursive hosting field — the runtime allocates an equal-size block or oversized span in the destination scope's dynamic region, relocates the live contents into it according to their ordinary move rules, updates the copied handle, and then returns the old source block or span to its exact-size stack. Anchored reference-type hosts inside a relocated block apply the same identity-merging rule as the outer host: a destination identity remains terminal and a distinct source identity forwards to it (§4.5). A promotion therefore completes before the source scope may drain and leaves no destination handle pointing into source-scope memory.

Relocation is **recursive**, because a relocated block may itself own dynamic blocks: a boxed payload holds its own boxed hosting members, and a backing store holds its elements' owned storage. Rehosting the root of a recursive structure therefore relocates the whole structure, at a cost proportional to the number of boxed nodes it contains rather than to the root alone. This is the ordinary consequence of the children being owned; a `List` already pays it for its backing store.

Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and tethers resolve identically regardless of physical placement (§4), because a tether follows the host's anchor rather than a fixed address.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-region-takes-the-boxes-and-a-box-asks-for-what-it-is) — "The region takes the boxes, and a box asks for what it is".

### 3.6 A handle has a fixed footprint; its payload lives in the dynamic region

Dynamically-sized reference types such as `List`, `String`, and similar types are represented as fixed-size **handles**. A handle records the payload's segmented offset and the metadata needed by the type, such as length and size class. The handle occupies a statically known footprint inline in the fixed-size region; its resizable backing store is a separate allocation in the dynamic region.

A type that contains a handle-typed field therefore stays statically sized:

```zane
type Inventory = #struct {
    items List<Item>;   // fixed-size handle inline; elements in the dynamic region
    count Int;
}
```

Dynamic block sizes are byte-based rather than element-type-based. A new list starts with a **128-byte block** — equivalent to sixteen 64-bit words — regardless of `T`. Its element capacity is `floor(block_bytes / stride(T))`. If one element does not fit in 128 bytes, the initial block is the smallest power-of-two block that can hold one element. Keeping the byte classes common allows blocks to be reused across lists with different element types, across other dynamically-sized reference types, and across boxed payloads.

A list grows according to the following rules:

1. When its capacity is exhausted, the requested block size is exactly twice its current block size.
2. The allocator first checks the size stack for that doubled size. If a block or oversized span is available, it is popped and the live elements are relocated into it.
3. If that stack is empty, the current backing store is the dynamic frontier allocation, the doubled size is at most 1 MiB, and the additional bytes fit before the current chunk boundary, the frontier is bumped by the additional bytes and the store grows in place.
4. Otherwise, a doubled block of at most 1 MiB is bump-allocated wholly inside one dynamic chunk. A doubled block larger than 1 MiB is allocated as a fresh dedicated oversized span (§3.1). The live elements are relocated into the new block or span.
5. After relocation, the handle's backing-store offset and size class are updated and the old block's base offset is pushed onto the stack for its exact old byte size.

A block never grows in place across a chunk boundary, and an oversized span is never extended in place: further growth relocates into a doubled oversized span after checking that exact-size stack first. Relocation moves or copies elements according to their type's ordinary move rules; the old block becomes reusable only after its previous occupants are no longer live. Guests to the list remain valid because they reach the list's host, whose fixed-size handle now names the current backing store.

A **boxed hosting member** (§3.3) uses the same two-part representation with a payload that never grows. Its handle records the payload's segmented offset; the payload is one reference-type instance, so it is allocated at **exactly that type's size**, aligned to that type's alignment requirement, and is returned to the size stack for that byte size when the member's occupant is destroyed or the member is overwritten (§3.2). Nothing is rounded up: a boxed payload has no size class, because a class exists to absorb growth and a boxed payload never grows. A payload larger than 1 MiB is a dedicated oversized span like any other. None of the growth rules above apply to it: a boxed payload is allocated once at its exact size and is only ever relocated by rehosting (§3.5).

The four classes below 128 bytes exist for these fixed-size payloads. Doubling from 128 bytes suits a buffer that will grow, but a boxed tree node is commonly two or three words, and rounding every node up to 128 bytes would leave most of each block unused for the whole of its life. Extending the classes downward keeps the region's reuse policy exactly as it is — one exact-size stack per class, shared across every dynamic payload in the scope — and adds only smaller classes to it.

Dynamic chunks and oversized spans begin at cache-line-aligned addresses. A **growable backing store** is 128 bytes or larger and cache-line aligned. A **boxed payload** is aligned to its type's alignment requirement, as that type would be aligned anywhere else. The frontier is rounded up to a block's alignment before it is bumped, so frontier allocations, reused blocks, and dedicated spans all keep their alignment without mixing payloads into fixed-size chunks.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-sentinel-that-costs-nothing-and-the-buffer-that-wanted-a-line) — "The sentinel that costs nothing, and the buffer that wanted a line".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".
> **Story:** [`stories/memory.md`](../stories/memory.md#the-region-takes-the-boxes-and-a-box-asks-for-what-it-is) — "The region takes the boxes, and a box asks for what it is".

### 3.7 Moving a value reuses the destination slot

A move transfers hosting into a destination host of the **same type** (see [`lifetimes.md`](lifetimes.md) §1). Because both sides have identical, statically known size, a move is a fixed-size overwrite of the destination slot:

- Moving into a fresh declaration or a return slot is in-place initialization.
- Moving into an already-initialized host first destroys the current occupant, then overwrites the same-size slot.

Moves only ever target the same or a higher scope ([`lifetimes.md`](lifetimes.md) §1.4), so the destination always outlives the source and its slot already exists. Rehosting copies the complete hosted representation into destination-owned storage. The inline payload or handle is copied into the destination's fixed-size slot. Each dynamic block the host owns — a backing store or a boxed payload — is relocated into an equal-size destination-region block or oversized span as specified in §3.5, recursively through any blocks it owns in turn; after its live contents and any contained host identities have been updated, the old block is returned to the source scope's exact-size stack. The source payload bytes then cease to be live. Its host-capable slot is rewritten into guest state and stores a tether to the terminal anchor; the rest of that full-size slot is dead until the slot is overwritten or its scope drains. If both source and destination already have distinct anchor identities, the destination identity remains terminal and the source identity becomes a forwarding anchor (§4.5). Existing tethers are never enumerated or rewritten.

---

## 4. Anchors and Tethers

### 4.1 The global anchor pool

Tethers are tracked through **anchor cells** in one runtime-global pool. An anchor cell occupies one 8-byte-aligned physical slot so every cell identity is representable by the shared 8-byte-word offset encoding. Its first `u32` is a segmented target offset; its second `u32` identifies the target as either a hosted payload or another anchor. A **payload anchor** terminates at the currently hosted value. A **forwarding anchor** preserves an older guest identity after two hosting identities merge, without changing any existing tether.

Anchor pages contain only equal-sized 8-byte slots. The pool therefore needs one free-address stack and one bump frontier rather than size classes. Pages are allocated lazily, never move, and remain mapped until runtime shutdown.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-the-cells-live-and-the-scan-that-pays-for-them) — "Where the cells live, and the scan that pays for them".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 4.2 Tethers are segmented offsets, not pointers

A tether is a **`u32` segmented offset** (§3.1) naming one global anchor cell — not a raw pointer and not a table index. At half the width of a 64-bit pointer, twice as many tethers fit in a cache line, and the 32-bit encoding keeps resolution on cheap 32-bit CPU math.

Every reference-type payload reserves a `u32` backpointer field initialized to `0`. Once an anchor exists, that field stores the terminal payload-anchor identity. Guests may store either that terminal identity or an older identity that forwards to it. Neither guests nor backpointers store the payload address directly.

An explicitly declared `&T` slot contains only this tether. A host-capable `T` slot that has been rehosted may use the same tether representation while it is in guest state, but retains enough storage to host another `T` later (§2.4).

The minimum physical footprint attributable to one directly tethered hosting lineage is **16 bytes**: one 4-byte tether, one 8-byte physical anchor slot, and one 4-byte payload backpointer. Each additional guest adds another 4-byte tether. Merging two already-anchored hosting identities allocates no new cell: the destination cell remains the payload anchor and the existing source cell becomes a forwarder until its former source scope drains.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 4.3 Anchors are created lazily

A hosting lineage that never gains a guest consumes no cell: its payload backpointer remains `0`. The first `&` taken on its host pops the global free-address stack if possible; otherwise it bump-allocates a cell at the global anchor frontier. The runtime writes the payload's current segmented offset into the cell and the cell's identity into the payload backpointer. Every later `&` from that host copies the backpointer.

> **Story:** [`stories/memory.md`](../stories/memory.md#finding-the-anchor-and-not-paying-when-there-are-no-refs) — "Finding the anchor, and not paying when there are no refs".

### 4.4 Resolving a tether

Resolving a tether uses the chunk directory to locate its global anchor cell. If the cell is a forwarder, resolution repeats with the target anchor until it reaches a payload anchor; it then resolves that cell's payload offset through the same directory and accesses the field. Forwarding chains cannot cycle because a move redirects a superseded source identity toward the same- or longer-lived destination identity. The runtime may path-compress visited forwarding cells. Because the pool never allocates cell identity `0`, resolving an untethered `0` traps rather than reading a live cell.

Consider reading a field through a tether, where `mainWeapon` is an `&Weapon`:

```zane
dps Float = mainWeapon.dps
```

The terminal case is tether → global anchor cell → payload offset → payload address → field. An older tether may first cross one or more forwarding anchor cells:

```text
mainWeapon: &Weapon
│
│ anchor identity
▼
chunk directory
│
▼
global anchor page
│
▼
anchor cell
│
│ current payload segmented offset
▼
chunk directory
│
▼
fixed-size chunk
│
▼
Weapon payload
│
│ ordinary field offset
▼
Weapon.dps
```

Ordinary overwrites and moves into untethered destinations update one payload anchor. When two anchored hosting identities merge, the destination anchor remains terminal and the source anchor forwards to it. Existing source guests therefore gain a forwarding hop, while destination guests and newly minted guests continue to use the terminal anchor directly. Assigning or passing a guest resolves its tether and stores the terminal identity in the new guest, so an obsolete identity cannot newly escape its former source-host scope.

The added cost over direct host access is one dependent anchor-cell load for a terminal tether and one load per uncompressed forwarding hop for an older tether. Across repeated accesses through the same guest with no intervening move or overwrite, the compiler may resolve the host address once and reuse it; the runtime may also compress the anchor path.

### 4.5 Moves and overwrites may merge anchor identities

An overwrite from a newly materialized value and a move from another host are distinct cases.

- **Ordinary overwrite:** if the destination hosting slot already has a payload anchor, the replacement payload inherits that backpointer and the cell is updated to the replacement's location. Existing destination guests therefore observe the new occupant. Destroying the old occupant does not return the cell, because the destination hosting identity continues.
- **Move into a fresh or untethered destination:** if the source already has a payload anchor, that cell follows the value into the destination and remains terminal. If no anchor exists but the moved-from source slot must remain readable as a guest, the runtime allocates one for the value after relocation. The source host-capable slot stores a tether to the terminal anchor.
- **Move into an anchored destination:** the destination payload anchor remains terminal, because the destination host identity survives replacement. If the source has a different payload anchor, the runtime changes that source cell into a forwarding anchor targeting the destination cell and records the forwarder on the former source scope's retirement stack. Existing source guests continue through the forwarding cell; existing destination guests continue directly through the destination cell. The moved payload stores the destination identity in its backpointer, and the moved-from source slot stores that same terminal tether. If resolving both identities already reaches the same terminal anchor, no new forwarding edge is installed.
- **Consumed untethered temporaries:** a temporary with no source slot that must remain readable may materialize into an untethered destination with backpointer `0` and allocate no anchor.

The same rules apply recursively to reference-type hosts contained in a relocated representation, including hosts inside dynamic backing stores. Their destination host identities survive replacement, and any distinct source identities forward to them. Anchor bookkeeping is O(1) for each merged host and never enumerates guests; physical rehosting remains proportional to the bytes, elements, and contained hosts relocated.

This is also how a moved-from symbol stays readable: after a move the host-capable symbol enters guest state and stores the terminal tether, so reads resolve through the anchor path to the value's new home (see [`lifetimes.md`](lifetimes.md) §1.6).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-move-problem-and-the-anchor-that-never-moves) — "The move problem, and the anchor that never moves".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 4.6 Payload and forwarding anchors retire at different events

A terminal payload anchor is returned to the global free-address stack when its **hosting identity** ends. Overwriting only the current occupant does not end that identity, because the destination host remains and existing destination guests follow the replacement. Rehosting transfers teardown responsibility to the destination host.

A source anchor converted into a forwarder may still be named by guests created before the move, so it is not returned when the source stops hosting. Instead, the runtime pushes it onto a retirement stack owned by the lexical scope of that former source host and returns it when that scope drains. Every guest that could already contain that obsolete identity is then dead by the ordinary scope rules. Assigning or passing such a guest stores the terminal identity (§2.6, §4.4), so the forwarding identity cannot newly escape its retirement scope.

Forwarding edges always point from a former source identity toward a destination identity in the same or a higher lexical scope. A forwarder therefore never depends on an anchor retired before it; at a shared scope drain all identities from that scope may be returned together. These retirement rules require neither reference counting nor guest enumeration. When either kind of anchor is returned, no live guest can still name it, so immediate reuse needs no generation counter, delayed reuse, or ABA protection.

> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 4.7 Why tethers never dangle or misdirect

A dangling or misdirected tether would require a guest to outlive the hosted value, a forwarding chain to cycle, or an anchor slot to be reused while an old guest remains. The model forbids all three. Scope checking prevents the first; forwarding always points from a superseded source identity toward a same- or longer-lived destination identity, preventing cycles; and the separate payload-anchor and forwarding-anchor retirement rules make slot reuse safe.

### 4.8 Resolution and allocation cost

The segmented encoding adds no meaningful arithmetic cost: the shift and mask that split a `u32` fold into machine addressing once the chunk base is loaded. A terminal tether pays one dependent anchor-cell load beyond direct host access; an older identity pays one additional load per uncompressed forwarding hop. Rehosting never enumerates guests, and path compression makes repeated traversal of a chain approach the terminal case. Physical relocation cost remains proportional to the representation moved.

A single global free stack and frontier require synchronization under concurrent allocation and teardown. Implementations may use thread-local anchor caches backed by the same global pool. The LIFO discipline of §3.2 describes how the central pool behaves, not a guarantee the language makes: which free slot a given anchor allocation receives is unobservable from the source language, so a cache that hands out slots in another order — or holds a returned slot until it flushes — changes nothing a program can detect. What such a cache **MUST NOT** change is anchor identity uniqueness, the retirement events of §4.6, or the lifetime guarantees that rest on them.

---

## 5. Language Comparisons

### 5.1 Hosting and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single host by default | ✅ | ❌ | ❌ | ✅ |
| Non-hosting guests as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Reference counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Guests remain usable across moves | ✅ via anchors | ❌ | ❌ | ⚠️ only when borrow checking permits the move pattern |
| Host overwrite keeps existing guests valid | ✅ via host/anchor indirection | ❌ | ❌ | ⚠️ heavily restricted by borrow checking |

### 5.2 Allocation

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Allocation strategy | per-scope fixed/dynamic arenas plus a global recyclable anchor pool | runtime-managed | allocator-dependent | allocator-dependent |

> **See also:** [`lifetimes.md`](lifetimes.md) §3 for the lifetime and destruction behavior comparison.

---

## 6. Summary

| Concept | Rule |
|---|---|
| Hosting storage | Reference-typed symbols, fields, and container elements are directly initialized and may later be overwritten |
| Value type | Mutable in place through a borrowed `mut` subject; storage may also be overwritten freely |
| `&` (guest) | Guest-only non-hosting storage; stores one tether, may be repointed, copied by value, and returned, but can never directly host a `T` |
| Host-capable guest state | After rehosting, the old hosted bytes cease to be live and a slot declared as `T` stores the terminal tether as a guest while retaining enough storage to host another `T` later |
| Place expression | Existing stable storage: a named symbol, a field access of a place, a place-projection subscript of a place, or an `&`/`'` parameter |
| New `&` value | May be minted only from a field access of a place or an `&` parameter; bare symbols, `[]` expressions, and temporaries are rejected |
| Guest source restriction | A bare symbol is a place but never a guest source (§2.8.1); a guest to a local's own hosting slot cannot be written, so overwriting that slot leaves no guest behind |
| `&` parameter | Declares that the caller must supply a guest source; the parameter is place-like inside the callee and may be stored or returned |
| Borrow | Non-hosting, non-escaping access to a caller's storage for the duration of a call; no anchor, not storable, not returnable, not a move-source |
| Value-type parameter | Always a read-only borrow; caller need not supply a place; copied only when bound into a fresh slot (assignment, declaration, field or return store) |
| Reference-type parameter | `T` swallows (hosting access; passing a host downgrades the caller's symbol to a guest whatever the body does — see [`lifetimes.md`](lifetimes.md) §1.8); `&T` takes a guest, which only a guest source can supply; `'T` borrows any place, bare symbols included, and leaves the caller a full host |
| `'T` position | Parameter positions only; never a storage, field, or return type |
| Reference-type `this` | Never a swallow position: bare `this T` is the borrow subject — `'` is never written on `this` — and `this &T` is a guest subject a method may store or return |
| Value-downstream enforcement | Value types may contain only primitives and other value types, transitively — never a reference (`#`) or `&` field |
| `&` targets reference types | An `&T` requires `T` to be a reference type; a value is shared by copy or scoped borrow, never by a stored `&` |
| Symbol declaration | Must be directly initialized |
| Reference-type placement | Inline storage is bump-allocated in the creating scope's fixed-size region; rehosting copies inline bytes and every owned dynamic block into destination-owned regions, recursively, before source storage is retired |
| Boxed hosting field | A hosting field whose type can lead back to the enclosing type is stored as a fixed-size handle inline with its instance in the dynamic region; the compiler chooses this because no finite inline layout exists, and nothing marks it in the source |
| `&` representation | A guest is represented internally by a `u32` tether: a segmented offset to a global anchor cell, which may terminate at a payload or forward to another anchor; `0` means no tether |
| Addressing | Scope chunks and global anchor pages share one `u32` segmented-offset directory; 8-byte-aligned offsets reach 32 GiB across up to 32768 1 MiB chunks |
| Untethered sentinel | `0`; the global anchor pool reserves this identity, while payloads may still occupy segmented offset `0` |
| Dynamic allocation | Exact-size stack first, frontier second; a growable backing store uses power-of-two sizes from 128 bytes because it doubles, while a boxed payload asks for exactly its type's size and has no class; blocks above 1 MiB use dedicated contiguous oversized spans |
| Dynamic-block alignment | A growable backing store is cache-line aligned; a boxed payload takes its type's alignment; the frontier is rounded up before it is bumped (§3.6) |
| Anchor cell | One global-pool 8-byte physical slot containing a `u32` target and a payload/forwarding kind; a forwarding cell targets another anchor |
| Backpointer | Each hosted payload stores the terminal payload-anchor identity for move updates and tether minting; `0` means no cell has been allocated |
| Anchor merging | Moving into an anchored destination preserves the destination anchor and converts a distinct source anchor into a forwarder; no guest is enumerated |
| Anchor lifecycle | A payload anchor returns when its hosting identity ends; a forwarding anchor returns when its former source-host scope drains |
| Anchor reuse safety | Guest canonicalization and lexical scope rules ensure no live tether names a returned slot |
| Tethered-instance cost | Minimum 16-byte direct footprint: one 4-byte tether, one 8-byte anchor slot, and one 4-byte backpointer; each retained historical identity uses one existing 8-byte forwarding slot until its retirement scope drains |

> **See also:** [`lifetimes.md`](lifetimes.md) §4 for the summary of scope, move, and destruction rules.
