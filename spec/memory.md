# Zane Memory Model

This document specifies Zane's memory model: hosting, guests, anchors, and arena layout. Lexical lifetime rules, rehosting, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for value and reference types. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling guests by combining single hosting, lexical lifetime rules, and anchor-based tracking.

- **`Overwritable hosts`.** A reference-type host is directly initialized and may later be overwritten.
- **`Guests ride on reference types`.** An `&` — a **guest** — is a non-hosting handle to a **reference type** (a `#`-marked type); a value type has no identity to anchor, so it is shared by copy or scoped borrow, never by a stored guest.
- **`Repointable guests`.** A guest is non-hosting storage that can point at different hosts over time.
- **`Lexical lifetime enforcement`.** Guest assignment and rehosting are checked using declaration scope alone (see [`lifetimes.md`](lifetimes.md) §1).
- **`Deterministic destruction`.** Objects are destroyed when their hosting scope drains; there is no tracing garbage collector (see [`lifetimes.md`](lifetimes.md) §2).
- **`Regioned arena placement`.** Every scope owns separate fixed-size and dynamic-backing-store regions. Statically sized storage is placed inline in the fixed-size region; resizable data uses the dynamic region. Anchors live outside scope arenas in one runtime-global fixed-slot pool (see §3 and §4).
- **`Segmented-offset tethers`.** Internally, each guest is represented by a `u32` tether — a chunk id plus an in-chunk offset — that points at the host's anchor cell, not a raw pointer (see §4.2).

The source language and runtime use separate terms: an object lives in a **host**, and a **guest** (`&T`) may access it without storing it or controlling its lifetime. Internally, each guest is represented by a **tether** that resolves through an **anchor**. Moving the object updates the anchor, so existing tethers — and therefore guests — continue to reach it.

These rules fit together mechanically. Hosts are the only storage that controls destruction. A guest may point only at an existing place, never a temporary. Lexical scope checks ensure the host outlives every guest derived from it. When an object is rehosted or a host is overwritten, guests stay valid. Internally, their tethers follow the host's anchor rather than a fixed object address.

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

Value types have no anchor and no heap identity. A value is mutated in place through a `mut` method whose receiver is a borrow of the value's storage (see [`effects.md`](effects.md) §2.3, [`functions.md`](functions.md) §2.4), and its storage slot may also be reassigned wholesale. Neither operation goes through the anchor system, because a value has no identity to track.

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

An `&` type is legal in storage sites (local symbols, fields, nested storage types), function parameter positions, and function return-type positions.

> **Story:** [`stories/memory.md`](../stories/memory.md#two-vocabularies-host-and-guest-above-anchor-and-tether) — "Two vocabularies: host and guest above anchor and tether".

### 2.5 Guests are repointable

An `&` symbol or `&` field may be assigned a different target later, as long as the scope rule in [`lifetimes.md`](lifetimes.md) §1.1 is satisfied.

### 2.6 Guests are independent

Assigning or passing a guest gives the destination its own guest to the same host. Rebinding one guest's storage site later changes only that storage site; it does not retarget other guests that already point to that host.

### 2.7 Guests and hosts use the same surface operations

At use sites, a guest is used with the same surface syntax as a direct host. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between host and guest matters only at the storage site: a guest stores a non-hosting link, while a host stores the object itself or its hosting slot.

### 2.8 Place expressions and new `&` values

A **place expression** is an expression that denotes an existing, stable storage location.

The following are place expressions:

- a named local, field-backed, or hosting/`&` storage symbol such as `engine`
- a field access whose base is a place, such as `car.engine` or `this.engine`
- a subscript expression `list[index]` when `list` is a place expression and `[]` is defined as a place projection for that receiver type
- an `&T` parameter inside the callee body (§2.9)

Only some place expressions may create a new guest. A new `&` binding may be initialized from:

- a named symbol
- a field access whose base is a place
- an `&T` parameter

A `[]` expression is never a source for creating a new `&`, even when it is a place expression.

Temporaries and other value-only expressions are not place expressions. Constructor calls and ordinary function results such as `Engine()` and `makeEngine()` are not places and cannot be bound to an `&`.

```zane
engine &Engine = Engine()   // ILLEGAL: Engine() is a temporary, not a place expression
```

```zane
engine Engine()
r &Engine = engine   // legal: engine is a named, stable storage location
```

```zane
weapons List = [Weapon(), Weapon()]
current &Weapon = weapons[1]   // ILLEGAL: `[]` cannot create a new `&`
```

```zane
first Weapon()
second Weapon()
weapons List = [first, second]
current &Weapon = weapons[1]   // legal: uses the existing stored `&Weapon`
```

This works because `weapons[1]` reads an `&Weapon` value that is already stored in the list. It does not create a new `&` from a hosting element. Those stored guests are stable because the language does not let `[]` create guests from host storage in the first place.

Non-`&` host bindings may be initialized from any expression, including temporaries. The host materializes the value into stable storage.

```zane
engine Engine()         // legal: plain host binding; Engine() temporary is materialized into engine
```

> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-new-ref-may-come-from) — "Where a new ref may come from".

### 2.9 Function parameters: borrows and `&`

A **borrow** is non-hosting, non-escaping access to a caller's storage for the duration of a call. Unlike a guest (§2.4), a borrow has no anchor, cannot be stored in a field, and cannot be returned; it exists only while the call runs. Borrowing is the passing mode for **value types**, which have no `&` of their own. A value-type parameter is a **read-only borrow** of the caller's slot, and a value is **copied** only when it is bound into a fresh slot — an assignment, a new declaration, or a field or return store. The one writable borrow is a value-type `mut` receiver (see [`functions.md`](functions.md) §2.4).

A **reference type** is passed through the hosting/`&` system instead, in one of two modes:

- A parameter declared as a plain reference type `T` **swallows** its argument — it takes the value by hosting access. The value belongs to the call-site scope, not the callee body ([`lifetimes.md`](lifetimes.md) §1.5), so it outlives the call. Passing a hosting value to such a parameter downgrades the caller's symbol to a guest ([`lifetimes.md`](lifetimes.md) §1.8), whatever the callee does with it — whether the verb relays the host back through its return or consumes it outright. A swallowing parameter the callee only reads downgrades the caller's host all the same; declaring it `&T` (a guest) is what keeps the caller as host.
- A parameter declared as `&T` is a **guest**: the caller supplies a source that may create a new guest under §2.8 (so `T` is a reference type, §2.4), and inside the callee body it acts as a place expression that may be stored into `&` storage or returned as `&T` under [`lifetimes.md`](lifetimes.md) §1.7. To read a reference-type object *without* taking hosting access, pass it as `&T`.

A reference-type `mut` receiver is neither of these: `this` is an implicit guest to the object, never swallowed, so it composes with `&T` parameters (see [`functions.md`](functions.md) §2.4).

Passing a value by borrow is the semantic model; where a read-only borrow is indistinguishable from a copy, the compiler may still pass a small value by copy, the same latitude placement has (§3.5). The distinction becomes observable under concurrent sharing, where a spawned reader sees the borrowed value live (see [`concurrency.md`](concurrency.md) §4.4).

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

// `&` parameter, read only: a reference-type object passed without consuming it
Int inspect(this Car, engine &Engine) {
    return this._value + engine.speed
}
```

Binding a plain (swallowed) parameter into `&` storage is illegal, because a swallowed value is hosted at the call site while an `&` field lives with the object that holds it — which may outlive the call, leaving the `&` dangling:

```zane
Unit setEngineWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: a swallowed host is not an `&` source
    return Unit()
}
```

This rule preserves uniform call syntax. The call site writes `consume(e)` or `inspect(e)` regardless of whether the parameter is `&`. The callee's signature determines whether an `&`-creating source is required from the caller.

### 2.10 Value-downstream enforcement (transitive value-only field restriction)

Value types form a closed world of plain value storage. A value-type field may contain primitives (see [`syntax.md`](syntax.md) §2.1) and other value types, but it **MUST NOT** contain a reference type (a `#`-marked type) or an `&`. This rule applies transitively: a value type containing another value type that eventually contains a reference-type or `&` field is also illegal. The same closure forbids a value type from recursing, since a self-reference would need indirection and indirection is a reference.

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
- The **dynamic region** stores the resizable backing stores behind handles such as `List` and `String`.

Each region is a separate chain of fixed-size **1 MiB chunks** mapped from the OS on demand. A chunk belongs to exactly one region: fixed-size slots and dynamic backing stores never coexist in the same chunk. A region maps no chunk until its first allocation. When its current chunk cannot satisfy an allocation, the runtime maps another chunk for that region, assigns it the next **chunk id**, and makes it current.

Scopes nest last-in-first-out, and their arenas nest with them: both regions of a scope are unmapped in full the moment the scope drains (§3.2, [`lifetimes.md`](lifetimes.md) §2.1). Arena granularity is an implementation choice, like boolean packing (§3.4) and placement (§3.5) — the compiler may fold several lexical scopes into one arena. What the language fixes is the observable behavior: a scope's memory is released together when that scope drains, and no guest ever resolves into released memory. A value that escapes is promoted out of the draining scope first (§3.5, §3.7), and its guests reach the promoted value through the canonical anchor (§4.5).

Anchors do not belong to any scope arena. The runtime owns one **global anchor pool**, implemented as a lazy chain of anchor-only 1 MiB pages. Every anchor occupies an **8-byte-aligned, 8-byte physical slot**: the first four bytes hold the `u32` payload offset and the remaining four bytes are reserved padding. An anchor page therefore contains 131072 addressable slots. The pool maps its first page only when the program creates its first guest and adds another page only when its current frontier and free-address stack cannot satisfy an allocation.

```text
one scope arena                        runtime-global anchor pool
──────────────────────────────         ──────────────────────────
fixed-size region   dynamic region     [anchor page] → [anchor page] → ...
[F1] → [F2]         [D1] → [D2]
```

An ordinary dynamic allocation never straddles a chunk boundary. A dynamic block of at most 1 MiB is wholly contained in one dynamic chunk; if the remaining bytes in the current chunk cannot hold it, allocation continues in a fresh dynamic chunk.

A dynamic block larger than 1 MiB is an **oversized span**: a dedicated contiguous OS mapping made from `block_size / 1 MiB` consecutive dynamic chunks, all belonging exclusively to that block and assigned consecutive chunk ids. Its handle stores the segmented offset of the span's first byte and its size class. After resolving that base, element addressing uses an ordinary byte offset across the contiguous mapping. Every constituent chunk also has a directory entry. Returning an oversized span pushes only its base offset onto the exact-size stack; the complete span remains mapped for reuse until the scope drains.

Scope chunks and global anchor pages draw ids from the same chunk directory, so payload locations, dynamic handles, tethers, backpointers, anchor cells, and size-stack entries all use one **`u32` segmented offset**:

```
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

The fixed-size region is a pure bump allocator: no size classes, no free list, no coalescing. A host has a fixed-size storage slot, so overwriting it destroys the current occupant and initializes the replacement directly in the same slot (§2.2, §3.7); the overwrite consumes no new space in the fixed-size region. Any dynamic backing stores owned by the destroyed occupant are returned to their exact-size stacks before the replacement becomes live. Nothing in the fixed-size region is reclaimed individually — bytes in a slot that cease to be live before the scope drains remain dead space until teardown.

The dynamic region adds exact-size reuse on top of its bump frontier. Dynamic blocks use power-of-two byte sizes beginning at **128 bytes**. Each scope maintains one LIFO **size stack** for every block size that has become reusable. To allocate a dynamic block of size `S`, the runtime first pops `size_stack[S]`; only when that stack is empty does it bump the dynamic frontier. It never satisfies a request from another size stack and never coalesces neighbouring blocks.

Returning a dynamic block pushes its base segmented offset onto the stack for that exact byte size. The stacks are shared by all dynamic types in the scope: a 128-byte block previously used by a `List<Int64>` may later hold string bytes or another list's elements. An oversized span participates in the same exact-size policy.

The global anchor pool has one LIFO **free-address stack**, because every anchor slot has the same size. Creating an anchor pops that stack first; only when it is empty does allocation bump the global anchor frontier, mapping another anchor page as needed. Returning an anchor pushes its segmented offset onto the same stack. Anchor pages remain mapped and retain their chunk-directory entries until runtime shutdown, including when every slot on a page is free; consequently every offset retained by the stack always resolves to its original anchor slot and anchor chunk ids are never repurposed during the run.

When a scope drains — after all its spawned work completes ([`concurrency.md`](concurrency.md) §4.1) — the runtime unmaps its fixed-size and dynamic chunks in bulk, with no per-object teardown pass threaded through the exit. Logical destruction timing is independent of this: a value dies when its host, container, or scope does ([`lifetimes.md`](lifetimes.md) §2.1); it is the *memory* that is reclaimed together at drain. Global anchor pages are not tied to scope teardown: individual slots are returned when their hosting lineages end (§4.6), while the pages themselves remain mapped until runtime shutdown.

> **Story:** [`stories/memory.md`](../stories/memory.md#when-the-free-stacks-fragment-and-the-arena-takes-the-scope) — "When the free stacks fragment, and the arena takes the scope".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 3.3 Value and reference layout follow declaration order

Fields are laid out in declaration order. Value types are stored inline. A statically sized reference-type instance is also stored inline in a fixed-size host slot, so value-type slots and reference-type host slots may sit directly beside each other in the fixed-size region. Reference types differ by identity and hosting semantics, not by requiring a separate indirect allocation.

A reference-type instance carries one `u32` backpointer field of anchor metadata (a segmented offset, §4.2) that remains `0` until the instance is first tethered. A dynamically-sized reference type such as `List` occupies a fixed-size handle inline in the same region; only the backing store named by that handle occupies the dynamic region (§3.6).

### 3.4 Booleans may be packed

The compiler may pack booleans in structs and arena frames when doing so does not change language semantics.

### 3.5 Statically sized storage uses the fixed-size region

Placement is an implementation decision, not a language-visible property. The arena model places every materialized, statically sized scope slot — value-type storage, a reference-type host, or a dynamic type's fixed-size handle — inline in that scope's fixed-size region. The compiler may keep an unobservable value in registers or otherwise optimize its physical placement, but reference types do not require a separate heap allocation merely because they carry identity.

When a reference-type instance is rehosted, all storage owned by that host is relocated into storage owned by the destination. Its statically sized inline bytes are copied into the destination host's fixed-size slot (§3.7). For every dynamic backing store, the runtime allocates an equal-size block or oversized span in the destination scope's dynamic region, relocates the live contents into it according to their ordinary move rules, updates the copied handle, and then returns the old source block or span to its exact-size stack. Anchored reference-type values hosted inside a relocated backing store update their own canonical anchor cells as their host locations change. A promotion therefore completes before the source scope may drain and leaves no destination handle pointing into source-scope memory.

Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and tethers resolve identically regardless of physical placement (§4), because a tether follows the host's anchor rather than a fixed address.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".

### 3.6 Handle-typed dynamic reference types have fixed footprint

Dynamically-sized reference types such as `List`, `String`, and similar types are represented as fixed-size **handles**. A handle records the backing store's segmented offset and the metadata needed by the type, such as length and size class. The handle occupies a statically known footprint inline in the fixed-size region; its resizable backing store is a separate allocation in the dynamic region.

A type that contains a handle-typed field therefore stays statically sized:

```zane
type Inventory = #struct {
    items List<Item>;   // fixed-size handle inline; elements in the dynamic region
    count Int;
}
```

Dynamic block sizes are byte-based rather than element-type-based. A new list starts with a **128-byte block** — equivalent to sixteen 64-bit words — regardless of `T`. Its element capacity is `floor(block_bytes / stride(T))`. If one element does not fit in 128 bytes, the initial block is the smallest power-of-two block that can hold one element. Keeping the byte classes common allows blocks to be reused across lists with different element types and across other dynamically-sized reference types.

A list grows according to the following rules:

1. When its capacity is exhausted, the requested block size is exactly twice its current block size.
2. The allocator first checks the size stack for that doubled size. If a block or oversized span is available, it is popped and the live elements are relocated into it.
3. If that stack is empty, the current backing store is the dynamic frontier allocation, the doubled size is at most 1 MiB, and the additional bytes fit before the current chunk boundary, the frontier is bumped by the additional bytes and the store grows in place.
4. Otherwise, a doubled block of at most 1 MiB is bump-allocated wholly inside one dynamic chunk. A doubled block larger than 1 MiB is allocated as a fresh dedicated oversized span (§3.1). The live elements are relocated into the new block or span.
5. After relocation, the handle's backing-store offset and size class are updated and the old block's base offset is pushed onto the stack for its exact old byte size.

A block never grows in place across a chunk boundary, and an oversized span is never extended in place: further growth relocates into a doubled oversized span after checking that exact-size stack first. Relocation moves or copies elements according to their type's ordinary move rules; the old block becomes reusable only after its previous occupants are no longer live. Guests to the list remain valid because they reach the list's host, whose fixed-size handle now names the current backing store.

Dynamic chunks, ordinary power-of-two blocks, and oversized spans begin at cache-line-aligned addresses. Because the minimum block is 128 bytes and every larger block doubles, frontier allocations, reused blocks, and dedicated spans preserve cache-line alignment without mixing backing stores into fixed-size chunks.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-sentinel-that-costs-nothing-and-the-buffer-that-wanted-a-line) — "The sentinel that costs nothing, and the buffer that wanted a line".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 3.7 Moving a value reuses the destination slot

A move transfers hosting into a destination host of the **same type** (see [`lifetimes.md`](lifetimes.md) §1). Because both sides have identical, statically known size, a move is a fixed-size overwrite of the destination slot:

- Moving into a fresh declaration or a return slot is in-place initialization.
- Moving into an already-initialized host first destroys the current occupant, then overwrites the same-size slot.

Moves only ever target the same or a higher scope ([`lifetimes.md`](lifetimes.md) §1.4), so the destination always outlives the source and its slot already exists. Rehosting copies the complete hosted representation into destination-owned storage. The inline payload or handle is copied into the destination's fixed-size slot. Each dynamic backing store is relocated into an equal-size destination-region block or oversized span as specified in §3.5; after its live contents and any contained anchor locations have been updated, the old store is returned to the source scope's exact-size stack. The source payload bytes then cease to be live. Its host-capable slot is rewritten into guest state and stores the canonical tether; the rest of that full-size slot is dead until the slot is overwritten or its scope drains. If the moved value is tethered, the destination inherits the same global anchor identity and updates its one cell (§4.5), never the tethers themselves.

---

## 4. Anchors and Tethers

### 4.1 The global anchor pool

Tethers are tracked through **anchor cells** in one runtime-global pool. An anchor cell has a 4-byte logical `u32` payload holding the current segmented offset (§3.1) of a hosted reference-type value, but occupies one 8-byte-aligned physical slot so every cell identity is representable by the shared 8-byte-word offset encoding. The cell's own segmented offset is the stable anchor identity for the complete hosting lineage, even when the value is rehosted across scopes.

Anchor pages contain only equal-sized 8-byte slots. The pool therefore needs one free-address stack and one bump frontier rather than size classes. Pages are allocated lazily, never move, and remain mapped until runtime shutdown.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-the-cells-live-and-the-scan-that-pays-for-them) — "Where the cells live, and the scan that pays for them".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 4.2 Tethers are segmented offsets, not pointers

A tether is a **`u32` segmented offset** (§3.1) naming one global anchor cell — not a raw pointer and not a table index. At half the width of a 64-bit pointer, twice as many tethers fit in a cache line, and the 32-bit encoding keeps resolution on cheap 32-bit CPU math.

Every reference-type payload reserves a `u32` backpointer field initialized to `0`. Once an anchor exists, that field stores the same stable anchor identity that its guests store. Guests and backpointers never store the payload address directly.

An explicitly declared `&T` slot contains only this tether. A host-capable `T` slot that has been rehosted may use the same tether representation while it is in guest state, but retains enough storage to host another `T` later (§2.4).

The minimum physical footprint attributable to one tethered hosting lineage is **16 bytes**: one 4-byte tether, one 8-byte physical anchor slot (containing a 4-byte cell payload), and one 4-byte payload backpointer. Each additional guest adds another 4-byte tether. Rehosting adds no forwarding metadata and, when only one side has live guests, no additional cell.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 4.3 Anchors are created lazily

A hosting lineage that never gains a guest consumes no cell: its payload backpointer remains `0`. The first `&` taken on its host pops the global free-address stack if possible; otherwise it bump-allocates a cell at the global anchor frontier. The runtime writes the payload's current segmented offset into the cell and the cell's identity into the payload backpointer. Every later `&` from that host copies the backpointer.

> **Story:** [`stories/memory.md`](../stories/memory.md#finding-the-anchor-and-not-paying-when-there-are-no-refs) — "Finding the anchor, and not paying when there are no refs".

### 4.4 Resolving a tether

Resolving a tether uses the chunk directory to locate the global anchor cell, reads the hosted payload's current segmented offset from that cell, resolves that offset through the same directory, then accesses the field. Because the pool never allocates cell identity `0`, resolving an untethered `0` traps rather than reading a live cell.

Consider reading a field through a tether, where `mainWeapon` is an `&Weapon`:

```zane
dps Float = mainWeapon.dps
```

The walk is always tether → global anchor cell → payload offset → payload address → field:

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

Moves, overwrites, and promotions update only the current payload offset in that same cell. Guests created before and after a promotion therefore follow an identical path, with no forwarding cells and no promotion-dependent extra hop.

The added cost over direct host access is one dependent anchor-cell load. Across repeated accesses through the same guest with no intervening move or overwrite, the compiler may resolve the host address once and reuse it.

### 4.5 Moves, overwrites, and rehosting keep one canonical anchor

An overwrite from a newly materialized value and a move from another host are distinct cases.

- **Ordinary overwrite:** if the destination hosting slot already has an anchor, the replacement payload inherits that backpointer and the cell is updated to the replacement's location. Existing destination guests therefore observe the new occupant. Destroying the old occupant does not return the cell, because the destination hosting lineage continues.
- **Move or rehosting with guests on only one side:** the anchor named by the live guest set becomes the destination's canonical anchor. If only the source has live guests, its anchor transfers to the destination. If only the destination has live guests, its anchor is preserved and the source's moved-from slot becomes another guest to it. Any noncanonical anchor left from an earlier, now-ended guest set is returned before the move completes. If neither side had live guests but the source slot remains readable in guest state, an anchor is allocated lazily for that new guest. The complete hosted representation is first relocated into destination-owned storage (§3.5, §3.7). The old source payload and backing-store bytes then cease to be live, the canonical cell is updated to the destination payload, the destination assumes teardown responsibility, and the source host-capable slot stores only its canonical tether in guest state.
- **Move or rehosting with live guests on both sides:** the program is ill-formed. The two guest sets name distinct stable identities, and a one-cell payload backpointer cannot preserve both through later moves without forwarding or guest enumeration. The compiler **MUST** reject the operation rather than recycle either referenced anchor. This restriction is determined from lexical guest liveness, not merely from whether a backpointer is nonzero.
- **Consumed untethered temporaries:** a temporary with no source slot that must remain readable may materialize into an untethered destination with backpointer `0` and allocate no anchor.

Anchor bookkeeping for every permitted operation is O(1) in the number of guests. Physical rehosting is proportional to the bytes or elements relocated and may update the canonical anchors of contained reference-type hosts, but it never enumerates guests. Promotion never creates a second live anchor path: it either preserves the sole live identity or is rejected. Source-scope and destination-scope guests therefore remain coherent after all later moves without repointing or forwarding.

This is also how a moved-from symbol stays readable: after a permitted move the host-capable symbol enters guest state and stores the canonical tether, so reads resolve through the anchor to the value's new home (see [`lifetimes.md`](lifetimes.md) §1.6).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-move-problem-and-the-anchor-that-never-moves) — "The move problem, and the anchor that never moves".
> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 4.6 Hosting-lifetime end returns the anchor

An anchor is returned to the global free-address stack when its **hosting lineage** ends. Overwriting only the current occupant does not end that lineage, because the host remains and existing guests follow the replacement. Rehosting transfers teardown responsibility to the destination host; the source slot is now a guest rather than a second host.

At the actual end of the hosting lineage, lexical scope rules guarantee that every guest capable of naming the anchor has already ceased to exist ([`lifetimes.md`](lifetimes.md) §1, [`concurrency.md`](concurrency.md) §4). The runtime may therefore recycle the slot immediately. No generation counter, delayed reuse, or ABA protection is required: a stale guest is not a representable program state.

> **Story:** [`stories/memory.md`](../stories/memory.md#two-payload-streams-and-the-anchor-that-leaves-the-scope) — "Two payload streams, and the anchor that leaves the scope".

### 4.7 Why tethers never dangle or misdirect

A dangling or misdirected tether would require a guest to outlive its host, an anchor cell to move, or an anchor slot to be reused while an old guest remains. The model forbids all three. Scope checking proves the first impossible; the global pool gives each live hosting lineage one stable cell identity; and the same scope rule makes immediate slot reuse safe after teardown.

### 4.8 Resolution and allocation cost

The segmented encoding adds no meaningful arithmetic cost: the shift and mask that split a `u32` fold into machine addressing once the chunk base is loaded. Tether resolution pays one dependent anchor-cell load beyond direct host access. Rehosting adds no forwarding hop and no guest-enumeration cost; its physical relocation cost remains proportional to the representation moved.

A single global free stack and frontier require synchronization under concurrent allocation and teardown. Implementations may use thread-local anchor caches backed by the same global pool without changing anchor identity, reuse order semantics, or lifetime guarantees.

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
| Value type | Mutable in place through a borrowed `mut` receiver; storage may also be overwritten freely |
| `&` (guest) | Guest-only non-hosting storage; stores one tether, may be repointed, copied by value, and returned, but can never directly host a `T` |
| Host-capable guest state | After rehosting, the old hosted bytes cease to be live and a slot declared as `T` stores the canonical tether as a guest while retaining enough storage to host another `T` later |
| Place expression | Existing stable storage: a named symbol, a field access of a place, a place-projection subscript of a place, or an `&` parameter |
| New `&` value | May be initialized only from a named symbol, a field access of a place, or an `&` parameter; temporaries and `[]` expressions are rejected |
| `&` parameter | Declares that the caller must supply an `&`-creating source; the parameter is place-like inside the callee |
| Borrow | Non-hosting, non-escaping access to a caller's storage for the duration of a call; the passing mode for value types; no anchor, not storable, not returnable |
| Value-type parameter | A read-only borrow; caller need not supply a place; copied only when bound into a fresh slot (assignment, declaration, field or return store) |
| Reference-type parameter | Plain `T` swallows (hosting access; passing a host downgrades the caller's symbol to a guest whatever the body does — see [`lifetimes.md`](lifetimes.md) §1.8); `&T` is a guest the caller lends while remaining host (may be stored into `&` storage or returned) |
| Reference-type `mut` receiver | `this` is an implicit `&` reference, never swallowed; composes with `&T` parameters |
| Value-downstream enforcement | Value types may contain only primitives and other value types, transitively — never a reference (`#`) or `&` field |
| `&` targets reference types | An `&T` requires `T` to be a reference type; a value is shared by copy or scoped borrow, never by a stored `&` |
| Symbol declaration | Must be directly initialized |
| Reference-type placement | Inline storage is bump-allocated in the creating scope's fixed-size region; rehosting copies inline bytes and every owned dynamic backing store into destination-owned regions before source storage is retired |
| `&` representation | A guest is represented internally by a `u32` tether: a segmented offset (chunk id + in-chunk offset) to the host's global anchor cell; `0` means no tether |
| Addressing | Scope chunks and global anchor pages share one `u32` segmented-offset directory; 8-byte-aligned offsets reach 32 GiB across up to 32768 1 MiB chunks |
| Untethered sentinel | `0`; the global anchor pool reserves this identity, while payloads may still occupy segmented offset `0` |
| Dynamic allocation | Power-of-two byte classes beginning at 128 bytes; exact-size stack first, frontier second; blocks above 1 MiB use dedicated contiguous oversized spans |
| Backing-store alignment | Dynamically-sized backing stores (§3.6) are cache-line-aligned; small inline allocations stay 8-byte aligned |
| Anchor cell | One global-pool 8-byte physical slot per tethered hosting lineage; its 4-byte `u32` payload holds the current payload segmented offset |
| Backpointer | Each hosted payload stores the stable `u32` identity of its anchor cell for move updates and tether minting; `0` means no cell has been allocated |
| Anchor lifecycle | Lazily allocated on first guest; preserved across overwrite and rehosting; returned to the global free-address stack when the hosting lineage ends |
| Anchor reuse safety | Immediate reuse is safe because lexical scope rules make a live stale guest unrepresentable |
| Move guest liveness | A move is rejected when both sides have live guests; with live guests on exactly one side, that side's anchor becomes the canonical one; with none on either side, a moved-from slot that stays readable anchors lazily (see [`lifetimes.md`](lifetimes.md) §1.10) |
| Tethered-instance cost | Minimum 16-byte physical footprint: one 4-byte tether, one 8-byte anchor slot, and one 4-byte backpointer |

> **See also:** [`lifetimes.md`](lifetimes.md) §4 for the summary of scope, move, and destruction rules.
