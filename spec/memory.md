# Zane Memory Model

This document specifies Zane's memory model: ownership, tethers, anchors, and arena layout. Lexical lifetime rules, ownership moves, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for value and reference types. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling tethers by combining single ownership, lexical lifetime rules, and anchor-based tracking of non-owning tethers.

- **`Overwritable owners`.** A reference-type owner is directly initialized and may later be overwritten.
- **`Tethers ride on reference types`.** An `&` — a **tether** — is a non-owning handle to a **reference type** (a `#`-marked type); a value type has no identity to anchor, so it is shared by copy or scoped borrow, never by a stored tether.
- **`Repointable tethers`.** A tether is non-owning storage that can point at different owners over time.
- **`Lexical lifetime enforcement`.** Tether assignment and ownership moves are checked using declaration scope alone (see [`lifetimes.md`](lifetimes.md) §1).
- **`Deterministic destruction`.** Objects are destroyed when their owning scope drains; there is no tracing garbage collector (see [`lifetimes.md`](lifetimes.md) §2).
- **`Arena placement`.** A reference-type instance is bump-allocated in the arena of the scope that creates it, and is copied into a parent arena only if it escapes that scope (see §3.5).
- **`Segmented-offset tethers`.** A tether is a `u32` segmented offset — a chunk id plus an in-chunk offset — that points at the owner's anchor cell, not a raw pointer (see §4.2).

These rules fit together mechanically. Owners are the only storage that controls destruction. A tether may point only at an existing place, never a temporary. Lexical scope checks ensure the owner outlives every tether derived from it. When ownership moves or an owner is overwritten, tethers stay valid because they follow the owner/anchor path rather than a fixed object address.

> **Story:** [`stories/memory.md`](../stories/memory.md#safety-without-a-collector-and-without-lifetimes) — "Safety without a collector and without lifetimes".

---

## 2. Ownership and Storage

### 2.1 Every reference-type instance has exactly one owner
Every instance of a reference type (a `#`-marked type, see [`types.md`](types.md) §2.1) is owned by exactly one symbol, field, or container slot at a time. Ownership is the default storage mode for reference values.

### 2.2 Reference-type owners are overwritable after initialization
Any owning storage position for a reference-type instance—a symbol, field, or container slot—**MUST** be directly initialized, and **MAY** later be overwritten.

```zane
tank Tank(...)
tank = Tank(...) // legal
```

Overwriting an owner does not invalidate existing tethers. Tethers follow the owner/anchor path, so later reads observe the owner's current value.

Container overwrite therefore does not depend on whether the element slot stores an owner or a tether. Both kinds of slots may be rewritten after initialization.

```zane
owners Array<Node, 2> = [Node(), Node()]
```

Rewriting `owners[1]` replaces the owned reference-type instance in that slot. Tethers to that slot observe the new value because tethers follow the owner/anchor path, not the original object.

### 2.3 Value types are mutable in place and freely overwritable
Value types have no anchor and no heap identity. A value is mutated in place through a `mut` method whose receiver is a borrow of the value's storage (see [`effects.md`](effects.md) §2.3, [`functions.md`](functions.md) §2.4), and its storage slot may also be reassigned wholesale. Neither operation goes through the anchor system, because a value has no identity to track.

```zane
pos Vec2(1, 2)
pos!setX(Float(3)) // in-place field write through a borrow of pos
pos = Vec2(3, 4)   // whole-slot overwrite
```

### 2.4 `&` is a tether: non-owning storage
`&` creates a **tether**: non-owning storage that points at a **reference type** only. An `&T` requires `T` to be a reference type — a declared `#struct`/`#variant`/`#enum` — because only a reference type carries the identity (the anchor, §4) that a stable, move-surviving tether needs. A value type is shared by copying it or by a scoped borrow (see [`functions.md`](functions.md) §2.4), never by a stored tether. Writing `&Node` names a tether to a reference type; a bare `&Int` over a value type is ill-formed.

A tether may be declared as:

- a local symbol
- a reference-type field
- an element type inside another storage type
- a function or constructor parameter
- a function return type

An `&` type is legal in storage sites (local symbols, fields, nested storage types), function parameter positions, and function return-type positions.

> **Story:** [`stories/memory.md`](../stories/memory.md#naming-the-tether) — "Naming the tether".

### 2.5 Tethers are repointable
An `&` symbol or `&` field may be assigned a different target later, as long as the scope rule in [`lifetimes.md`](lifetimes.md) §1.1 is satisfied.

### 2.6 Tethers are independent
Assigning or passing a tether gives the destination its own tether to the same owner. Rebinding one tether's storage site later changes only that storage site; it does not retarget other tethers that already point to that owner.

### 2.7 Tethers and owners use the same surface operations
At use sites, a tether is used with the same surface syntax as a direct owner. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between owner and tether matters only at the storage site: a tether stores a non-owning link, while an owner stores the object itself or its owning slot.

### 2.8 Place expressions and new `&` values
A **place expression** is an expression that denotes an existing, stable storage location.

The following are place expressions:

- a named local, field-backed, or owning/`&` storage symbol such as `engine`
- a field access whose base is a place, such as `car.engine` or `this.engine`
- a subscript expression `list[index]` when `list` is a place expression and `[]` is defined as a place projection for that receiver type
- an `&T` parameter inside the callee body (§2.9)

Only some place expressions may create a new tether. A new `&` binding may be initialized from:

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

This works because `weapons[1]` reads an `&Weapon` value that is already stored in the list. It does not create a new `&` from an owning element. Those stored tethers are stable because the language does not let `[]` create tethers from owner storage in the first place.

Non-`&` owner bindings may be initialized from any expression, including temporaries. The owner materializes the value into stable storage.

```zane
engine Engine()         // legal: plain owner binding; Engine() temporary is materialized into engine
```

> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-new-ref-may-come-from) — "Where a new ref may come from".

### 2.9 Function parameters: borrows and `&`
A **borrow** is non-owning, non-escaping access to a caller's storage for the duration of a call. Unlike a tether (§2.4), a borrow has no anchor, cannot be stored in a field, and cannot be returned; it exists only while the call runs. Borrowing is the passing mode for **value types**, which have no `&` of their own. A value-type parameter is a **read-only borrow** of the caller's slot, and a value is **copied** only when it is bound into a fresh slot — an assignment, a new declaration, or a field or return store. The one writable borrow is a value-type `mut` receiver (see [`functions.md`](functions.md) §2.4).

A **reference type** is passed through the ownership/`&` system instead, in one of two modes:

- A parameter declared as a plain reference type `T` **swallows** its argument: ownership moves into the callee.
- A parameter declared as `&T` is a **tether**: the caller supplies a source that may create a new tether under §2.8 (so `T` is a reference type, §2.4), and inside the callee body it acts as a place expression that may be stored into `&` storage or returned as `&T` under [`lifetimes.md`](lifetimes.md) §1.7. To read a reference-type object *without* consuming it, pass it as `&T`.

A reference-type `mut` receiver is neither of these: `this` is an implicit tether to the object, never swallowed, so it composes with `&T` parameters (see [`functions.md`](functions.md) §2.4).

Passing a value by borrow is the semantic model; where a read-only borrow is indistinguishable from a copy, the compiler may still pass a small value by copy, the same latitude placement has (§3.5). The distinction becomes observable under concurrent sharing, where a spawned reader sees the borrowed value live (see [`concurrency.md`](concurrency.md) §4.4).

```zane
type Car = #struct {
    engine &Engine;   // an `&` field
    spare Engine;     // an owned field
    _value Int;
}

// `&` parameter is a tether; it may be stored into an `&` field
Void setEngine(this Car, engine &Engine) mut {
    this.engine = engine
}

// plain reference-type parameter swallows: ownership moves into an owned field
Void setSpare(this Car, engine Engine) mut {
    this.spare = engine
}

// `&` parameter, read only: a reference-type object passed without consuming it
Int inspect(this Car, engine &Engine) {
    return this._value + engine.speed
}
```

Binding a plain (swallowed) parameter into `&` storage is illegal, because the swallowed value is frame-local and an `&` into it would outlive it:

```zane
Void setEngineWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: a swallowed owner is not an `&` source
}
```

This rule preserves uniform call syntax. The call site writes `consume(e)` or `inspect(e)` regardless of whether the parameter is `&`. The callee's signature determines whether an `&`-creating source is required from the caller.

### 2.10 Value-downstream enforcement (transitive value-only field restriction)
Value types form a closed world of plain value storage. A value-type field may contain primitives (see [`syntax.md`](syntax.md) §2.1) and other value types, but it **MUST NOT** contain a reference type (a `#`-marked type) or an `&`. This rule applies transitively: a value type containing another value type that eventually contains a reference-type or `&` field is also illegal. The same closure forbids a value type from recursing, since a self-reference would need indirection and indirection is a reference.

Here, **downstream** means "through nested value-type fields." The restriction is checked recursively through the full value graph.

Value types are copied and overwritten as ordinary inline values. They do not have per-instance anchors or destruction tracking. If a value could contain a reference-type field, copying it would silently duplicate ownership. If a value could contain an `&`, copying it would silently duplicate non-owning tracking state without going through the anchor system. Downstream enforcement keeps value copying mechanical, keeps ownership/tether bookkeeping confined to reference types, and — because nothing reachable from a value can be aliased — is what lets a value be shared by snapshot and mutated concurrently under [`concurrency.md`](concurrency.md) §4.

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
The runtime does not reserve one flat region. Each lexical scope owns a **bump arena**: a chain of fixed-size **1 MiB chunks** mapped from the OS on demand. Allocation advances one frontier pointer inside the current chunk; when a chunk fills, the runtime maps a fresh 1 MiB chunk, assigns it the next **chunk id**, and makes it current. Growing an arena never copies or relocates live data.

Scopes nest last-in-first-out, and their arenas nest with them: a scope's chunks are unmapped in full the moment the scope drains (§3.2, [`lifetimes.md`](lifetimes.md) §2.1). Arena granularity is an implementation choice, like boolean packing (§3.4) and placement (§3.5) — the compiler may fold several lexical scopes into one arena. What the language fixes is the observable behavior: memory a scope allocates outlives every tether taken on it and is released together when the scope drains.

Within an arena, payloads and anchor cells (§4.1) occupy **separate regions** — distinct chunk chains — so a scan over payloads never strides across interleaved cell metadata. Both chains draw chunk ids from the same directory, so a segmented offset addresses either identically.

Every in-arena location is a **`u32` segmented offset**, never a native pointer. The `u32` splits into two fields:

```
   u32 segmented offset
  ┌───────────────┬─────────────────────────┐
  │   chunk id    │   in-chunk word offset   │
  │  (high bits)  │       (low bits)         │
  └───────────────┴─────────────────────────┘
```

Allocations are 8-byte aligned, so the low bits count 8-byte words: a 1 MiB chunk holds 2¹⁷ words, so **17 low bits** address any slot in a chunk and the remaining **15 high bits** select one of up to 32768 live chunks — a reach of 32 GiB. A small **chunk directory** maps a chunk id to that chunk's native base address, so an address is materialized only at use, as `directory[chunk id] + word offset × 8`: splitting the `u32` is a shift and a mask, and the directory lookup is one load. Tethers (§4.2), the per-owner backpointer (§4.2), and the anchor cells (§4.1) are all `u32` segmented offsets. The value `0` — chunk `0`, word `0` — is the *untethered* sentinel. It costs no reserved memory: because anchor cells are allocated only in the anchor-cell region (§4.1), which never includes that slot, no cell is ever at `0`, so a `0` backpointer or tether can never name a real cell. Owner payloads carry no such restriction and may occupy offset `0` — so a scope's first payload sits at a chunk base, which is why the frontier needs no reserved gap.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 3.2 Allocation is a bump; teardown is an unmap
Within a scope's arena, allocation is a single frontier bump — no size classes, no free list, no coalescing. Owners are not reclaimed one at a time: an owner that dies or is overwritten mid-scope (§2.2) becomes dead space in the arena until the scope drains. Reclamation is bulk. When the scope drains — after all its spawned work completes ([`concurrency.md`](concurrency.md) §4.1) — the runtime unmaps the scope's chunks and every byte the scope held is released at once, with no per-object teardown pass threaded through the exit. Logical destruction timing is unchanged — an owner dies when its owner, container, or scope does ([`lifetimes.md`](lifetimes.md) §2.1); it is the *memory* that is reclaimed together at drain.

> **Story:** [`stories/memory.md`](../stories/memory.md#when-the-free-stacks-fragment-and-the-arena-takes-the-scope) — "When the free stacks fragment, and the arena takes the scope".

### 3.3 Value and reference layout follow declaration order
Fields are laid out in declaration order. Value types are stored inline. A reference-type instance has stable identity and carries one `u32` backpointer field of anchor metadata (a segmented offset, §4.2) that stays `0` until the instance is first tethered. Arena placement of a reference-type instance is covered in §3.5.

### 3.4 Booleans may be packed
The compiler may pack booleans in structs and arena frames when doing so does not change language semantics.

### 3.5 Reference-type instances are placed in their scope's arena
Placement is an implementation decision, not a language-visible property. A reference-type instance is bump-allocated in the arena of the scope that creates it when both hold:

- its size is statically known, and
- it does not escape that scope in a way a move cannot satisfy.

When an instance escapes — it is moved into a longer-lived owner in a parent scope — it is **promoted**: its payload is copied into the destination scope's arena (§3.7). A dynamically-sized instance forces its backing store into the arena the same way (§3.6). Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and tethers resolve identically regardless of which arena the instance lives in (§4), because a tether resolves through the owner's anchor cell rather than a fixed address. This freedom mirrors the boolean-packing rule (§3.4): the compiler may choose the cheaper arena whenever doing so cannot change program meaning.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".

### 3.6 Handle-typed core reference types have fixed footprint
The core dynamically-sized reference types — `List`, `String`, and similar types — are represented as a fixed-size **handle**: a small header (or single segmented offset) whose dynamic backing store lives in the arena. The handle occupies a statically known footprint wherever it is stored.

A type that contains a handle-typed field therefore stays statically sized. A type holding a `List` field does not become dynamically sized; it stores the fixed handle inline, and only the backing store behind the handle is a separate arena allocation.

```zane
type Inventory = #struct {
    items List<Item>;   // fixed-size handle inline; backing store in the arena
    count Int;
}
```

This is what keeps arena placement (§3.5) broadly applicable: almost every value is statically sized at its own level, so dynamic size appears only inside the backing stores of handle types.

A backing store is allocated **cache-line-aligned**: before it is placed the arena frontier is advanced to the next cache-line boundary. A backing store is streamed and grown in bulk, and an unaligned base would let its elements straddle cache lines, so sequential access would touch a line more than it needs. Aligning the base packs whole elements within lines. Small inline allocations keep the ordinary 8-byte alignment (§3.1) — cache-line-aligning every small object would waste most of a line per object for no locality gain, since the cost only arises when streaming across many elements. The padding to reach the boundary is at most one line, negligible against a backing store's size.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-sentinel-that-costs-nothing-and-the-buffer-that-wanted-a-line) — "The sentinel that costs nothing, and the buffer that wanted a line".

### 3.7 Moving a value reuses the destination slot
A move transfers ownership into a destination owner of the **same type** (see [`lifetimes.md`](lifetimes.md) §1). Because both sides have identical, statically known size, a move is a fixed-size overwrite of the destination slot:

- Moving into a fresh declaration or a return slot is in-place initialization.
- Moving into an already-initialized owner first destroys the current occupant, then overwrites the same-size slot.

Moves only ever target the same or a higher scope ([`lifetimes.md`](lifetimes.md) §1.4), so the destination always outlives the source and its slot already exists. A move into a higher scope copies the inline bytes into the destination scope's arena — a promotion (§3.5). Because handle-typed fields (§3.6) keep the moved footprint small, a move relocates only the inline bytes — a handle's backing store never moves. If the moved value is tethered, the move also updates its one anchor cell (§4.5), never the tethers themselves.

---

## 4. Anchors and Tethers

### 4.1 The anchor cell
Tethers are tracked through per-owner **anchor cells** rather than one shared table. An anchor cell is a single **`u32`** holding the current segmented offset (§3.1) of one tethered owner; it stores nothing else. A cell is an ordinary arena allocation — bump-allocated on the owner's first tether (§4.3) — so there is no monolithic table to relocate as anchors accumulate: minting an anchor is one bump, never a resize.

Cells are bump-allocated in a **dedicated anchor-cell region** of the scope's arena, a separate chunk chain from the one holding payloads (§3.1). Keeping cells out of the payload stream means a scan over payloads never strides across interleaved cell metadata, so iteration stays dense and a payload's placement never depends on how many of its neighbours were tethered first. The cell region is itself compact and heavily reused, so resolving through a cell (§4.4) is a load into hot, cache-resident memory.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-the-cells-live-and-the-scan-that-pays-for-them) — "Where the cells live, and the scan that pays for them".

### 4.2 Tethers are segmented offsets, not pointers
A tether is a **`u32` segmented offset** (§3.1) pointing at the owner's anchor cell — not a raw pointer and not a table index. At half the width of a 64-bit pointer, twice as many tethers fit in a cache line, and the 32-bit encoding keeps resolution on cheap 32-bit CPU math. A cell is allocated only on the first tether of an owner (§4.3), so cells stay a small fraction of live memory, and the `u32`'s 32 GiB reach (§3.1) sits far beyond any realistic working set.

The value `0` (chunk `0`, word `0`, §3.1) means *untethered*. A cell is never placed at `0` (§4.1, §3.1), so `0` is never a real cell, and a stray resolution of an untethered `0` traps rather than reading live memory.

Every reference-type instance reserves a **`u32` backpointer** field, initialized to `0`; the first tether records the segmented offset of the instance's anchor cell there. The cell is allocated lazily (§4.3), whereas the backpointer field is always present in the layout, so object size is fixed and array layout stays uniform. The backpointer lets the owner mint new tethers — `&x` copies the offset — and lets a move locate and update the owner's cell (§4.5). It is a single offset, not a list of tethers: the owner never enumerates the tethers that point at it, which is what keeps moves O(1) (§4.5).

A tethered reference-type instance therefore costs **12 bytes** across the whole chain: the 4-byte tether (wherever it is stored), the 4-byte anchor cell, and the 4-byte backpointer in the payload.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 4.3 Anchors are created lazily
An owner that is never tethered consumes no cell: its backpointer field stays `0` and no cell is allocated (it still carries the 4-byte field, §4.2). The first `&` taken on an owner bump-allocates a cell in the arena, writes the owner's current segmented offset into it, and records the cell's own segmented offset in the owner's backpointer. Every subsequent `&` of that owner copies the backpointer.

> **Story:** [`stories/memory.md`](../stories/memory.md#finding-the-anchor-and-not-paying-when-there-are-no-refs) — "Finding the anchor, and not paying when there are no refs".

### 4.4 Resolving a tether
Resolving a tether reads the anchor cell it points at, reads the owner's segmented offset from that cell, materializes the owner address through the chunk directory (§3.1), then accesses the field. Because a cell is never at `0`, a resolution of an untethered `0` never reads a live cell.

Consider reading a field through a tether, where `mainWeapon` is an `&Weapon`:

```zane
dps Float = mainWeapon.dps
```

`mainWeapon` holds a segmented offset to an anchor cell, not the Weapon's address. Field access uses `.`: it resolves the cell, reads the owner's current offset from it, resolves that offset to the owner's address, then adds the field offset. The walk is tether → cell → owner offset → owner address → field:

```
  chunk directory  (chunk id → native base; small, hot, register/L1-resident)

  mainWeapon : &Weapon  =  segmented offset ──▶ anchor cell

                       ┌── anchor cell (u32) ──┐
                       │     owner offset       │
                       └───────────┼───────────┘
                                   │  cell = owner segmented offset
                                   ▼
        owner address  =  directory[owner chunk id] + owner word offset × 8
                                   │
                                   ▼
                       ┌────────────────────────┐
                       │  Weapon  (in an arena)  │
                       │  name        String     │
                       │  dps         Float ◀─ read │ = *(owner address + offset(dps))
                       │  range       Float      │
                       │  backpointer u32        │
                       └────────────────────────┘
```

The `.` reads the field; it never reassigns the Weapon. Rebinding `mainWeapon` itself would only repoint the tether at a different owner's cell (subject to the scope rule in [`lifetimes.md`](lifetimes.md) §1.1) — it would not overwrite any field. Splitting each segmented offset is a shift and a mask that fold into machine addressing once the chunk base is in hand, so the encoding costs no arithmetic over a raw-pointer dereference. The added cost is one dependent load: the cell read between the tether and the field, and because cells live packed together in the arena's compact anchor-cell region (§4.1) that load normally lands in hot, cache-resident memory. See §4.8.

### 4.5 Moves, overwrites, and promotion update one cell, not all tethers
A tether follows the owner/anchor path rather than pointing at a fixed object address. When an owner is overwritten in place (§2.2) or moved within its scope, the runtime writes the payload's new segmented offset into its one anchor cell, located through the backpointer (§4.2). The cell itself does not move, so every existing tether — which points at the cell, not the payload — observes the owner's current location on its next resolution with no per-tether fixup.

**Promotion** on escape (§3.5) carries one extra step, because the owner's anchor cell lives in the anchor-cell region of the scope that minted it (§4.1) — a scope that is about to drain. Every tether that already points at that cell was taken in that scope or deeper ([`lifetimes.md`](lifetimes.md) §1.1), so none of them outlives the cell. On promotion the runtime therefore does two things: it updates the old cell to the payload's new location, so those existing tethers keep resolving to the live promoted copy for the remainder of the source scope, and it **resets the payload's backpointer to `0`**. The reset re-arms lazy allocation (§4.3): the next tether taken in the destination scope mints a fresh cell in the destination arena's cell region — one that lives exactly as long as the promoted value. The old cell and the tethers reading it then expire together when the source scope drains.

This is why relocation, overwrite, and promotion are all **O(1) with respect to the number of tethers**. It is also how a moved-from symbol stays readable: after a move the symbol downgrades to an `&` — a segmented offset to the cell — and reads resolve through the cell to the value's new home (see [`lifetimes.md`](lifetimes.md) §1.6).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-move-problem-and-the-anchor-that-never-moves) — "The move problem, and the anchor that never moves". [`stories/memory.md`](../stories/memory.md#where-the-cells-live-and-the-scan-that-pays-for-them) — "Where the cells live, and the scan that pays for them".

### 4.6 Teardown releases cells in bulk
Anchor cells are arena allocations, so they are never individually freed. When a scope drains, its chunks — payload and anchor-cell regions alike — are unmapped (§3.2) and its cells vanish together with the owners and payloads they served.

Because scope rules keep every tether inside its owner's lifetime ([`lifetimes.md`](lifetimes.md) §1.1, §1.4), no live tether can point at a cell that has been unmapped. Destruction therefore creates no dangling-tether state.

### 4.7 Why tethers never dangle
A dangling tether would require one of three failures: an owner overwrite breaking existing tethers, a tether outliving the owner's scope, or an object move leaving tethers pointed at a dead address. The model eliminates each. Owner/anchor indirection makes overwrite and move follow the current cell value instead of a stale address (§4.5). The same-or-higher-scope rule keeps every tether inside the owner's lifetime envelope ([`lifetimes.md`](lifetimes.md) §1.1). The model is enforced by storage shape and lexical scope, not by runtime borrow tracking.

### 4.8 Resolution cost
The segmented encoding adds no arithmetic cost: the shift and mask that split a `u32` into a chunk id and a word offset fold into machine addressing once the chunk base is loaded. The chunk directory is the hottest table in the program — tiny, and normally resident in registers or L1 — so materializing an address from a segmented offset is effectively a single indexed load and add.

The genuine cost of any anchor scheme is **one extra dependent load per tether resolution** — the cell read — versus an idealized raw pointer that cannot survive moves. Because cells are packed together in the arena's compact anchor-cell region (§4.1), that load usually lands in hot, cache-resident memory, a few cycles at most. It is paid only when resolving a tether; direct access to an owner never consults a cell. Across a run of accesses through the same tether with no intervening move, overwrite, or promotion, the compiler resolves the owner address once and reuses it, so hot loops do not re-pay the load.

---

## 5. Language Comparisons

### 5.1 Ownership and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner by default | ✅ | ❌ | ❌ | ✅ |
| Non-owning tethers as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Reference counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Tethers remain usable across moves | ✅ via anchors | ❌ | ❌ | ⚠️ only when borrow checking permits the move pattern |
| Owner overwrite keeps existing tethers valid | ✅ via owner/anchor indirection | ❌ | ❌ | ⚠️ heavily restricted by borrow checking |

### 5.2 Allocation

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Allocation strategy | per-scope bump arenas, bulk teardown | runtime-managed | allocator-dependent | allocator-dependent |

> **See also:** [`lifetimes.md`](lifetimes.md) §3 for the lifetime and destruction behavior comparison.

---

## 6. Summary

| Concept | Rule |
|---|---|
| Owning storage | Reference-typed symbols, fields, and container elements are directly initialized and may later be overwritten |
| Value type | Mutable in place through a borrowed `mut` receiver; storage may also be overwritten freely |
| `&` (tether) | Non-owning storage; may be repointed, copied by value, and returned from functions |
| Place expression | Existing stable storage: a named symbol, a field access of a place, a place-projection subscript of a place, or an `&` parameter |
| New `&` value | May be initialized only from a named symbol, a field access of a place, or an `&` parameter; temporaries and `[]` expressions are rejected |
| `&` parameter | Declares that the caller must supply an `&`-creating source; the parameter is place-like inside the callee |
| Borrow | Non-owning, non-escaping access to a caller's storage for the duration of a call; the passing mode for value types; no anchor, not storable, not returnable |
| Value-type parameter | A read-only borrow; caller need not supply a place; copied only when bound into a fresh slot (assignment, declaration, field or return store) |
| Reference-type parameter | Plain `T` swallows (ownership moves in); `&T` is a reference (may be stored into `&` storage or returned) |
| Reference-type `mut` receiver | `this` is an implicit `&` reference, never swallowed; composes with `&T` parameters |
| Value-downstream enforcement | Value types may contain only primitives and other value types, transitively — never a reference (`#`) or `&` field |
| `&` targets reference types | An `&T` requires `T` to be a reference type; a value is shared by copy or scoped borrow, never by a stored `&` |
| Symbol declaration | Must be directly initialized |
| Reference-type placement | Bump-allocated in the creating scope's arena; promoted to a parent arena only on escape — an unobservable choice |
| `&` representation | A `u32` segmented offset (chunk id + in-chunk offset) to the owner's anchor cell; `0` means untethered |
| Addressing | Every location is a `u32` segmented offset resolved through the chunk directory; 8-byte-aligned offsets reach 32 GiB across up to 32768 1 MiB chunks |
| Untethered sentinel | `0` (chunk `0`, word `0`); costs no reserved memory because cells never occupy it — payloads may sit at offset `0` |
| Backing-store alignment | Dynamically-sized backing stores (§3.6) are cache-line-aligned so sequential element access does not straddle lines; small inline allocations stay 8-byte aligned |
| Anchor cell | One `u32` per tethered owner holding its current segmented offset; bump-allocated in the scope's dedicated anchor-cell region, kept out of the payload stream so payload iteration stays dense |
| Backpointer | Each tethered owner stores the `u32` segmented offset of its anchor cell for move updates and tether minting |
| Anchor lifecycle | Lazily allocated on first tether; on promotion the payload re-anchors in the destination arena; released in bulk when the owner's scope drains |
| Tethered-instance cost | 12 bytes total: the 4-byte tether, the 4-byte anchor cell, and the 4-byte backpointer |

> **See also:** [`lifetimes.md`](lifetimes.md) §4 for the summary of scope, move, and destruction rules.
