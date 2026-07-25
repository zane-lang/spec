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
- **`Regioned arena placement`.** Every scope owns separate fixed-size, dynamic-backing-store, and anchor-cell regions. Statically sized storage is placed inline in the fixed-size region; resizable data uses the dynamic region (see §3).
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
Void setEngine(this Car, engine &Engine) mut {
    this.engine = engine
}

// plain reference-type parameter: taken by hosting access, then moved into a hosting field of this
Void setSpare(this Car, engine Engine) mut {
    this.spare = engine
}

// `&` parameter, read only: a reference-type object passed without consuming it
Int inspect(this Car, engine &Engine) {
    return this._value + engine.speed
}
```

Binding a plain (swallowed) parameter into `&` storage is illegal, because a swallowed value is hosted at the call site while an `&` field lives with the object that holds it — which may outlive the call, leaving the `&` dangling:

```zane
Void setEngineWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: a swallowed host is not an `&` source
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

### 3.1 Scope arenas and segmented offsets
The runtime does not reserve one flat region. Each lexical scope owns an **arena** made from three independent allocation regions:

- The **fixed-size region** stores materialized value-type slots, statically sized reference-type hosts, and the fixed-size handles of dynamic core types.
- The **dynamic region** stores the resizable backing stores behind handles such as `List` and `String`.
- The **anchor-cell region** stores the scope-local anchor cells created for tethered hosts (§4.1).

Each region is a separate chain of fixed-size **1 MiB chunks** mapped from the OS on demand. A chunk belongs to exactly one region: fixed-size slots, dynamic backing stores, and anchor cells never coexist in the same chunk. A region maps no chunk until its first allocation. When its current chunk cannot satisfy an allocation, the runtime maps another chunk for that region, assigns it the next **chunk id**, and makes it current. Growing one region never copies or relocates allocations in another.

```text
one scope arena

fixed-size region        dynamic region           anchor-cell region
─────────────────        ──────────────           ──────────────────
[fixed chunk]            [dynamic chunk]          [anchor chunk]
[fixed chunk] → ...      [dynamic chunk] → ...    [anchor chunk] → ...
```

The chains are lazy and independent. A scope that uses no dynamic backing store maps no dynamic chunk; a scope that never creates a guest maps no anchor-cell chunk. When the scope drains, every chunk belonging to all three regions is unmapped together. The compiler may optimize away or coalesce physically unobservable storage, but it **MUST** preserve region exclusivity, lifetime, and drain behavior.

All three regions draw chunk ids from the same chunk directory, so every in-arena location uses the same **`u32` segmented offset**. The `u32` splits into two fields:

```
   u32 segmented offset
  ┌───────────────┬─────────────────────────┐
  │   chunk id    │   in-chunk word offset   │
  │  (high bits)  │       (low bits)         │
  └───────────────┴─────────────────────────┘
```

Allocations are at least 8-byte aligned, so the low bits count 8-byte words: a 1 MiB chunk holds 2¹⁷ words, so **17 low bits** address any slot in a chunk and the remaining **15 high bits** select one of up to 32768 live chunks — a reach of 32 GiB. The chunk directory maps a chunk id to the chunk's native base address, so an address is materialized only at use as `directory[chunk id] + word offset × 8`.

Tethers (§4.2), per-host backpointers (§4.2), anchor cells (§4.1), dynamic handles, and size-stack entries (§3.2) use segmented offsets. The value `0` — chunk `0`, word `0` — is the *untethered* sentinel. It costs no reserved memory because anchor cells are allocated only in the anchor-cell region, which never contains that location. Fixed-size payloads may occupy offset `0`.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 3.2 Allocation is a bump; teardown is an unmap
The fixed-size and anchor-cell regions are pure bump allocators: allocation advances the region's frontier, with no size classes, free lists, or coalescing. A host is a fixed-size storage slot, so overwriting it destroys the current occupant and initializes the replacement directly in the same slot (§2.2, §3.7); it consumes no new arena space.

The dynamic region adds exact-size reuse on top of its bump frontier. Dynamic blocks use power-of-two byte sizes beginning at **128 bytes**. Each scope maintains one LIFO **size stack** for every block size that has become reusable. To allocate a dynamic block of size `S`, the runtime first pops `size_stack[S]`; only when that stack is empty does it bump the dynamic frontier, mapping more dynamic chunks as needed. It never satisfies a request from a different size stack and never coalesces neighbouring free blocks.

Returning a dynamic block pushes its segmented offset onto the stack for that exact byte size. These stacks are shared by all dynamic types in the scope: a 128-byte block previously used by a `List<Int64>` may later hold string bytes or another list's elements. The stacks affect allocation within the scope only; they require no per-object reclamation when the scope drains.

Reclamation remains bulk. When the scope drains — after all its spawned work completes ([`concurrency.md`](concurrency.md) §4.1) — the runtime unmaps every fixed-size, dynamic, and anchor-cell chunk owned by the scope, with no reachability scan or per-object memory-reclamation pass. Anything that escaped was already placed in storage whose lifetime covers its destination host (§3.5). Logical destruction timing is unchanged — a host dies when its host, container, or scope does ([`lifetimes.md`](lifetimes.md) §2.1); the underlying region memory is released together at drain.

> **Story:** [`stories/memory.md`](../stories/memory.md#when-the-free-stacks-fragment-and-the-arena-takes-the-scope) — "When the free stacks fragment, and the arena takes the scope".

### 3.3 Value and reference layout follow declaration order
Fields are laid out in declaration order. Value types are stored inline. A statically sized reference-type instance is also stored inline in a fixed-size host slot, so value-type slots and reference-type host slots may sit directly beside each other in the fixed-size region. Reference types differ by identity and hosting semantics, not by requiring a separate indirect allocation.

A reference-type instance carries one `u32` backpointer field of anchor metadata (a segmented offset, §4.2) that remains `0` until the instance is first tethered. A dynamic core type such as `List` occupies a fixed-size handle inline in the same region; only the backing store named by that handle occupies the dynamic region (§3.6).

### 3.4 Booleans may be packed
The compiler may pack booleans in structs and arena frames when doing so does not change language semantics.

### 3.5 Statically sized storage uses the fixed-size region
Placement is an implementation decision, not a language-visible property. The arena model places every materialized, statically sized scope slot — value-type storage, a reference-type host, or a dynamic type's fixed-size handle — inline in that scope's fixed-size region. The compiler may keep an unobservable value in registers or otherwise optimize its physical placement, but reference types do not require a separate heap allocation merely because they carry identity.

When a reference-type instance is rehosted into a longer-lived destination, its statically sized inline bytes are copied into the destination host's fixed-size slot (§3.7). A dynamic backing store is semantically owned by the current host. The compiler **MUST** place that store in a dynamic region whose lifetime covers every destination into which the handle can be rehosted, so rehosting transfers ownership of the same backing store without copying it. Only growth of the dynamic value may relocate the backing store (§3.6).

Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and tethers resolve identically regardless of physical placement (§4), because a tether follows the host's anchor rather than a fixed address.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".

### 3.6 Handle-typed core reference types have fixed footprint
The core dynamically-sized reference types — `List`, `String`, and similar types — are represented as fixed-size **handles**. A handle records the backing store's segmented offset and the metadata needed by the type, such as length and size class. The handle occupies a statically known footprint inline in the fixed-size region; its resizable backing store is a separate allocation in the dynamic region.

A type that contains a handle-typed field therefore stays statically sized:

```zane
type Inventory = #struct {
    items List<Item>;   // fixed-size handle inline; elements in the dynamic region
    count Int;
}
```

Dynamic block sizes are byte-based rather than element-type-based. A new list starts with a **128-byte block** — equivalent to sixteen 64-bit words — regardless of `T`. Its element capacity is `floor(block_bytes / stride(T))`. If one element does not fit in 128 bytes, the initial block is the smallest power-of-two block that can hold one element. Keeping the byte classes common allows blocks to be reused across lists with different element types and across other dynamic core types.

A list grows according to the following rules:

1. When its capacity is exhausted, the requested block size is exactly twice its current block size.
2. The allocator first checks the size stack for that doubled size. If a block is available, it is popped and the live elements are relocated into it.
3. If that stack is empty and the current backing store is the dynamic frontier allocation with enough contiguous room to double, the frontier is bumped by the additional bytes and the store grows in place.
4. Otherwise, the doubled block is allocated by bumping the dynamic frontier, mapping more dynamic chunks as needed, and the live elements are relocated into it.
5. After relocation, the handle's backing-store offset and size class are updated and the old block's offset is pushed onto the size stack for its old byte size.

Relocation moves or copies elements according to their type's ordinary move rules; the old block becomes reusable only after its previous occupants are no longer live. Guests to the list remain valid because they reach the list's host, whose fixed-size handle now names the current backing store.

Dynamic chunks and all power-of-two blocks begin at cache-line-aligned addresses. Because the minimum block is 128 bytes and every larger block doubles, both frontier allocations and reused blocks preserve cache-line alignment without mixing backing stores into fixed-size chunks.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-sentinel-that-costs-nothing-and-the-buffer-that-wanted-a-line) — "The sentinel that costs nothing, and the buffer that wanted a line".

### 3.7 Moving a value reuses the destination slot
A move transfers hosting into a destination host of the **same type** (see [`lifetimes.md`](lifetimes.md) §1). Because both sides have identical, statically known size, a move is a fixed-size overwrite of the destination slot:

- Moving into a fresh declaration or a return slot is in-place initialization.
- Moving into an already-initialized host first destroys the current occupant, then overwrites the same-size slot.

Moves only ever target the same or a higher scope ([`lifetimes.md`](lifetimes.md) §1.4), so the destination always outlives the source and its slot already exists. A move into a higher scope copies the inline bytes into the destination scope's fixed-size region — a promotion (§3.5). Because handle-typed fields (§3.6) keep the moved footprint small, rehosting copies only the handle and transfers ownership of the same backing store; rehosting itself never relocates that store. A dynamic store changes address only through the growth procedure in §3.6. If the moved value is tethered, the move also updates its one anchor cell (§4.5), never the tethers themselves.

---

## 4. Anchors and Tethers

### 4.1 The anchor cell
Tethers are tracked through per-host **anchor cells** rather than one shared table. An anchor cell is a single **`u32`** holding the current segmented offset (§3.1) of one hosted object; it stores nothing else.

Anchor storage is **scope-local**, never global. Each scope owns a dedicated anchor-cell region — a separate lazy chunk chain from both its fixed-size and dynamic regions. A scope that never creates a guest allocates no anchor chunk. The first tether to a host bump-allocates its cell in that scope's anchor region (§4.3); minting another cell is one bump and never resizes a monolithic table.

Keeping cells out of the other two streams preserves dense fixed-size layout and prevents dynamic-buffer history from affecting anchor placement. The region remains compact and heavily reused while live, and all of its chunks disappear with the scope in the same bulk unmap as the other regions.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-the-cells-live-and-the-scan-that-pays-for-them) — "Where the cells live, and the scan that pays for them".

### 4.2 Tethers are segmented offsets, not pointers
A tether is a **`u32` segmented offset** (§3.1) pointing at the host's anchor cell — not a raw pointer and not a table index. At half the width of a 64-bit pointer, twice as many tethers fit in a cache line, and the 32-bit encoding keeps resolution on cheap 32-bit CPU math. A cell is allocated only on the first tether of a host (§4.3), so cells stay a small fraction of live memory, and the `u32`'s 32 GiB reach (§3.1) sits far beyond any realistic working set.

The value `0` (chunk `0`, word `0`, §3.1) means *untethered*. A cell is never placed at `0` (§4.1, §3.1), so `0` is never a real cell, and a stray resolution of an untethered `0` traps rather than reading live memory.

Every reference-type instance reserves a **`u32` backpointer** field, initialized to `0`; the first tether records the segmented offset of the instance's anchor cell there. The cell is allocated lazily (§4.3), whereas the backpointer field is always present in the layout, so object size is fixed and array layout stays uniform. The backpointer lets a host mint new tethers from the object — `&x` copies the offset — and lets a move locate and update the object's cell (§4.5). It is a single offset, not a list of tethers: the runtime never enumerates the tethers that point at the object, which is what keeps moves O(1) (§4.5).

A tethered reference-type instance therefore costs **12 bytes** across the whole chain: the 4-byte tether (wherever it is stored), the 4-byte anchor cell, and the 4-byte backpointer in the payload.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 4.3 Anchors are created lazily
A hosted object that never gains a tether consumes no cell: its backpointer field stays `0` and no cell is allocated (it still carries the 4-byte field, §4.2). The first `&` taken on its host bump-allocates a cell in the arena, writes the hosted object's current segmented offset into it, and records the cell's own segmented offset in the object's backpointer. Every subsequent `&` from that host copies the backpointer.

> **Story:** [`stories/memory.md`](../stories/memory.md#finding-the-anchor-and-not-paying-when-there-are-no-refs) — "Finding the anchor, and not paying when there are no refs".

### 4.4 Resolving a tether
Resolving a tether reads the anchor cell it points at, reads the hosted object's segmented offset from that cell, materializes the object's address through the chunk directory (§3.1), then accesses the field. Because a cell is never at `0`, a resolution of an untethered `0` never reads a live cell.

Consider reading a field through a tether, where `mainWeapon` is an `&Weapon`:

```zane
dps Float = mainWeapon.dps
```

`mainWeapon` holds a segmented offset to an anchor cell, not the Weapon's address. Field access uses `.`: it resolves the cell, reads the hosted object's current offset from it, resolves that offset to the object's address, then adds the field offset. The walk is tether → cell → payload offset → payload address → field:

#### Illustrative resolution walkthrough

A tether contains a segmented offset to an anchor cell, not directly to the
hosted object. Reading `mainWeapon.dps` therefore resolves two segmented offsets.

```text
mainWeapon: &Weapon
│
│ tether segmented offset
│ [ anchor-cell chunk id | anchor-cell word offset ]
▼
chunk directory
│
▼
anchor-cell chunk
│
▼
anchor cell
│
│ hosted object segmented offset
│ [ payload chunk id | payload word offset ]
▼
chunk directory
│
▼
payload chunk
│
▼
Weapon payload
│
│ ordinary field offset within Weapon
▼
Weapon.dps
```

The first segmented offset locates the anchor cell. The cell contains the
second segmented offset, which locates the hosted object's current payload.
Both use the same chunk-directory resolution rule.

If the `Weapon` moves or its host is overwritten, the runtime updates only
the hosted object's offset stored in the anchor cell. `mainWeapon` continues
to point at the same cell, and its next access reaches the payload's new location.

The `.` reads the field; it never reassigns the Weapon. Rebinding `mainWeapon` itself would only repoint the tether at a different host's cell (subject to the scope rule in [`lifetimes.md`](lifetimes.md) §1.1) — it would not overwrite any field. Splitting each segmented offset is a shift and a mask that fold into machine addressing once the chunk base is in hand, so the encoding costs no arithmetic over a raw-pointer dereference. The added cost is one dependent load: the cell read between the tether and the field, and because cells live packed together in the arena's compact anchor-cell region (§4.1) that load normally lands in hot, cache-resident memory. See §4.8.

### 4.5 Moves, overwrites, and promotion update one cell, not all tethers
A tether follows the host/anchor path rather than pointing at a fixed object address. When a host is overwritten in place (§2.2) or its object is moved within the scope, the runtime writes the payload's new segmented offset into the object's one anchor cell, located through the backpointer (§4.2). The cell itself does not move, so every existing tether — which points at the cell, not the payload — observes the hosted object's current location on its next resolution with no per-tether fixup.

**Promotion** on escape (§3.5) carries one extra step, because the hosted object's anchor cell lives in the anchor-cell region of the scope that minted it (§4.1) — a scope that is about to drain. Every tether that already points at that cell was taken in that scope or deeper ([`lifetimes.md`](lifetimes.md) §1.1), so none of them outlives the cell. On promotion the runtime therefore does two things: it updates the old cell to the payload's new location, so those existing tethers keep resolving to the live promoted copy for the remainder of the source scope, and it **resets the payload's backpointer to `0`**. The reset re-arms lazy allocation (§4.3): the next tether taken in the destination scope mints a fresh cell in the destination arena's cell region — one that lives exactly as long as the promoted value. The old cell and the tethers reading it then expire together when the source scope drains.

This is why relocation, overwrite, and promotion are all **O(1) with respect to the number of tethers**. It is also how a moved-from symbol stays readable: after a move the symbol downgrades to an `&` — a segmented offset to the cell — and reads resolve through the cell to the value's new home (see [`lifetimes.md`](lifetimes.md) §1.6).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-move-problem-and-the-anchor-that-never-moves) — "The move problem, and the anchor that never moves". [`stories/memory.md`](../stories/memory.md#where-the-cells-live-and-the-scan-that-pays-for-them) — "Where the cells live, and the scan that pays for them".

### 4.6 Teardown releases cells in bulk
Anchor cells are arena allocations, so they are never individually freed. When a scope drains, its chunks — payload and anchor-cell regions alike — are unmapped (§3.2) and its cells vanish together with the hosts and payloads they served.

Because scope rules keep every tether inside its host's lifetime ([`lifetimes.md`](lifetimes.md) §1.1, §1.4), no live tether can point at a cell that has been unmapped. Destruction therefore creates no dangling-tether state.

### 4.7 Why tethers never dangle
A dangling tether would require one of three failures: a host overwrite breaking existing tethers, a tether outliving the host's scope, or an object move leaving tethers pointed at a dead address. The model eliminates each. Host/anchor indirection makes overwrite and move follow the current cell value instead of a stale address (§4.5). The same-or-higher-scope rule keeps every tether inside the host's lifetime envelope ([`lifetimes.md`](lifetimes.md) §1.1). The model is enforced by storage shape and lexical scope, not by runtime borrow tracking.

### 4.8 Resolution cost
The segmented encoding adds no arithmetic cost: the shift and mask that split a `u32` into a chunk id and a word offset fold into machine addressing once the chunk base is loaded. The chunk directory is the hottest table in the program — tiny, and normally resident in registers or L1 — so materializing an address from a segmented offset is effectively a single indexed load and add.

The genuine cost of any anchor scheme is **one extra dependent load per tether resolution** — the cell read — versus an idealized raw pointer that cannot survive moves. Because cells are packed together in the arena's compact anchor-cell region (§4.1), that load usually lands in hot, cache-resident memory, a few cycles at most. It is paid only when resolving a tether; direct access through a host never consults a cell. Across a run of accesses through the same tether with no intervening move, overwrite, or promotion, the compiler resolves the host address once and reuses it, so hot loops do not re-pay the load.

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
| Allocation strategy | per-scope bump arenas, bulk teardown | runtime-managed | allocator-dependent | allocator-dependent |

> **See also:** [`lifetimes.md`](lifetimes.md) §3 for the lifetime and destruction behavior comparison.

---

## 6. Summary

| Concept | Rule |
|---|---|
| Hosting storage | Reference-typed symbols, fields, and container elements are directly initialized and may later be overwritten |
| Value type | Mutable in place through a borrowed `mut` receiver; storage may also be overwritten freely |
| `&` (guest) | Non-hosting storage; may be repointed, copied by value, and returned from functions |
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
| Reference-type placement | Bump-allocated in the creating scope's arena; promoted to a parent arena only on escape — an unobservable choice |
| `&` representation | A guest is represented internally by a `u32` tether: a segmented offset (chunk id + in-chunk offset) to the host's anchor cell; `0` means no tether |
| Addressing | Every location is a `u32` segmented offset resolved through the chunk directory; 8-byte-aligned offsets reach 32 GiB across up to 32768 1 MiB chunks |
| Untethered sentinel | `0` (chunk `0`, word `0`); costs no reserved memory because cells never occupy it — payloads may sit at offset `0` |
| Backing-store alignment | Dynamically-sized backing stores (§3.6) are cache-line-aligned so sequential element access does not straddle lines; small inline allocations stay 8-byte aligned |
| Anchor cell | One `u32` per hosted object that has at least one tether, holding the object's current segmented offset; bump-allocated in the scope's dedicated anchor-cell region, kept out of the payload stream so payload iteration stays dense |
| Backpointer | Each hosted object stores the `u32` segmented offset of its anchor cell for move updates and tether minting; `0` means no cell has been allocated |
| Anchor lifecycle | Lazily allocated on first guest; on promotion the payload re-anchors in the destination arena; released in bulk when the host's scope drains |
| Tethered-instance cost | 12 bytes total: the 4-byte tether, the 4-byte anchor cell, and the 4-byte backpointer |

> **See also:** [`lifetimes.md`](lifetimes.md) §4 for the summary of scope, move, and destruction rules.
