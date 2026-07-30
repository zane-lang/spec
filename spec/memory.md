# Zane Memory Model

This document specifies Zane's memory model: hosting, guests, borrows, and arena layout. Lexical lifetime rules, moves, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for value and reference types. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling guests by never relocating a hosted object and by restricting where a guest may be rooted.

- **`Hosted objects never move`.** A reference-type instance occupies one storage slot for its entire life. Nothing relocates it, so a reference to it never needs fixing up.
- **`Overwritable hosts`.** A reference-type host is directly initialized and may later be overwritten. An overwrite destroys the current occupant and constructs the replacement in the same slot.
- **`Guests ride on reference types`.** An `&` — a **guest** — is a non-hosting handle to a **reference type** (a `#`-marked type); a value type has no identity to point at, so it is shared by copy or borrow, never by a stored guest.
- **`Guests are rooted in immovable storage`.** A guest may be created only from a field, a stored guest, or a guest parameter — never from a bare symbol, because a bare symbol is the only storage a move can empty (see §2.8).
- **`Borrows cover the rest`.** A `'T` — a **borrow** — is non-hosting, non-escaping access for the duration of one call. It may be rooted in anything, including a bare symbol, because it cannot outlive the call (see §2.9).
- **`Regioned arena placement`.** Every scope owns separate fixed-size and dynamic-backing-store regions. Statically sized storage is placed in the fixed-size region; resizable data uses the dynamic region (see §3).
- **`Segmented-offset references`.** Internally, a guest holds a `u32` **segmented offset** — a chunk id plus an in-chunk offset — naming the storage it reaches (see §4).

The source language and the runtime use the same small vocabulary. A **host** is the storage that holds an object and governs when it is destroyed; a **guest** (`&T`) may reach the object without controlling its lifetime; a **borrow** (`'T`) may reach it only for the duration of a call. Because a hosted object never moves, a guest is an ordinary reference and resolving one is a single lookup.

These rules fit together mechanically. Hosting is the only thing that controls destruction, and exactly one storage position hosts an object at a time. Moving transfers hosting between storage positions without relocating the object. Overwriting replaces a host's occupant while the storage itself persists. A guest may be rooted only in storage that a move cannot empty and that no container operation can relocate, and lexical scope checks ensure that storage outlives every guest reaching it.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-move-that-stopped-relocating) — "The move that stopped relocating".

---

## 2. Hosting and Storage

### 2.1 Every reference-type instance has exactly one host

Every instance of a reference type (a `#`-marked type, see [`types.md`](types.md) §2.1) is hosted by exactly one symbol, field, or container slot at a time. Hosting is the default storage mode for reference values, and it is what decides when the instance is destroyed ([`lifetimes.md`](lifetimes.md) §2.1).

Hosting is a **static** property. The compiler knows at every program point which storage position hosts a given instance, because the only operation that transfers hosting is a move, and moves are confined to the declaration block of the symbol being moved ([`lifetimes.md`](lifetimes.md) §1.3).

### 2.2 Reference-type hosts are overwritable after initialization

Any hosting storage position for a reference-type instance—a symbol, field, or container slot—**MUST** be directly initialized, and **MAY** later be overwritten.

```zane
tank Tank(...)
tank = Tank(...) // legal
```

An overwrite destroys the current occupant and constructs the replacement **in the same storage**. The slot persists across the overwrite, so existing guests remain valid and later reads through them observe the new occupant.

```zane
hosts Array<Node, 2> = [Node(), Node()]
```

Rewriting `hosts[1]` replaces the reference-type instance occupying that element's slot. Guests reaching that slot observe the new value.

### 2.3 Value types are mutable in place and freely overwritable

Value types have no identity. A value is mutated in place through a `mut` method whose receiver is a borrow of the value's storage (see [`effects.md`](effects.md) §2.3, [`functions.md`](functions.md) §2.4), and its storage slot may also be reassigned wholesale.

```zane
pos Vec2(1, 2)
pos!setX(Float(3)) // in-place field write through a borrow of pos
pos = Vec2(3, 4)   // whole-slot overwrite
```

### 2.4 `&` is a guest: non-hosting storage

`&` creates a **guest**: non-hosting storage that points at a **reference type** only. An `&T` requires `T` to be a reference type — a declared `#struct`/`#variant`/`#enum` — because only a reference type has the identity a stored reference names. A value type is shared by copying it or by a borrow (§2.9), never by a stored guest. Writing `&Node` names a guest to a reference type; a bare `&Int` over a value type is ill-formed.

A guest holds a reference to the hosted object's storage and nothing else. It does not extend the object's lifetime, and it cannot become a host.

A guest may be declared as:

- a local symbol
- a reference-type field
- an element type inside another storage type
- a function or constructor parameter
- a function return type

An `&` type is legal in storage sites (local symbols, fields, nested storage types), function parameter positions, and function return-type positions.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".

### 2.5 Guests are repointable

An `&` symbol or `&` field may be assigned a different target later, as long as the source rule in §2.8 and the scope rule in [`lifetimes.md`](lifetimes.md) §1.1 are satisfied.

### 2.6 Guests are independent

Assigning or passing a guest gives the destination its own guest to the same object. Rebinding one guest's storage site later changes only that storage site; it does not retarget other guests that already point at that object.

### 2.7 Guests and hosts use the same surface operations

At use sites, a guest is used with the same surface syntax as a direct host. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between host and guest matters only at the storage site: a guest stores a non-hosting reference, while a host stores the reference that governs the object's lifetime.

### 2.8 Place expressions and guest sources

A **place expression** is an expression that denotes an existing, stable storage location.

The following are place expressions:

- a named local, field-backed, or hosting/`&` storage symbol such as `engine`
- a field access whose base is a place, such as `car.engine` or `this.engine`
- a subscript expression `list[index]` when `list` is a place expression and `[]` is defined as a place projection for that receiver type
- an `&T` or `'T` parameter inside the callee body (§2.9)

Not every place expression may create a guest. A new `&` binding may be initialized only from:

- a field access whose base is a place
- an `&T` parameter

A **bare symbol MUST NOT** be a guest source. A bare symbol — a local binding or a parameter named directly by an identifier expression — is the only storage a move can empty ([`lifetimes.md`](lifetimes.md) §1.2), so a guest rooted there could outlive the object it was created from. A field, by contrast, is never a move-source: it can be overwritten, but an overwrite leaves a live occupant in the same storage (§2.2), so a guest rooted in a field always resolves.

```zane
engine Engine()
r &Engine = engine   // ILLEGAL: a bare symbol cannot be a guest source
```

```zane
car Car(Engine())
r &Engine = car.engine   // legal: a field is never emptied by a move
```

To reach an object held by a bare symbol without taking hosting, pass it as a borrow (§2.9). To hold a lasting reference to it, give it a host that is not a bare symbol.

A `[]` expression is never a source for creating a new `&`, and **neither is any path rooted in one**: `cars[1]` and `cars[1].engine` are both rejected. A container's own operations drop elements and relocate its backing store as it grows (§3.6), so element storage is the one place a guest could be left naming bytes that have moved. Temporaries and other value-only expressions are not place expressions at all: constructor calls and ordinary function results such as `Engine()` and `makeEngine()` have no stable storage for a guest to name.

```zane
engine &Engine = Engine()   // ILLEGAL: Engine() is a temporary, not a place expression
```

```zane
weapons List = [Weapon(), Weapon()]
current &Weapon = weapons[1]          // ILLEGAL: `[]` cannot create a new `&`
barrel &Barrel = weapons[1].barrel   // ILLEGAL: the path is rooted in a `[]`
```

Reading an `&` that is *already stored* in a container is not creating one, and remains legal:

```zane
current &Weapon = arsenal.weapons[1]   // legal: reads a stored `&Weapon`
```

Non-`&` host bindings may be initialized from any expression, including temporaries. The host materializes the value into its own storage.

```zane
engine Engine()         // legal: plain host binding; the temporary is materialized into engine's storage
```

> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".

### 2.9 Function parameters: swallow, guest, and borrow

A reference-type parameter is written in one of three modes, and the mode is part of the signature. Nothing in the callee's body changes what the signature already states ([`lifetimes.md`](lifetimes.md) §1.8).

| Mode | Written | Caller supplies | Callee may |
|---|---|---|---|
| Swallow | `T` | any move-source | take hosting; the caller's symbol downgrades |
| Guest | `&T` | a guest source (§2.8) | store it in `&` storage or return it |
| Borrow | `'T` | any place expression, including a bare symbol | read and mutate for the duration of the call only |

A **swallowing** parameter takes its argument by hosting access. The value belongs to the call-site scope, not the callee body ([`lifetimes.md`](lifetimes.md) §1.5), so it outlives the call. Passing a host to such a parameter downgrades the caller's symbol ([`lifetimes.md`](lifetimes.md) §1.8).

A **guest** parameter is an `&T`. The caller must supply a guest source under §2.8 — which, because a bare symbol is not one, means a field access or another guest. Inside the callee body the parameter is a place expression that may be stored into `&` storage or returned as `&T` under [`lifetimes.md`](lifetimes.md) §1.7.

A **borrow** parameter is a `'T`: non-hosting, non-escaping access to the caller's storage for the duration of the call. A borrow has no storage of its own, **MUST NOT** be stored in a field or container, and **MUST NOT** be returned. Because it cannot outlive the call, it may be rooted in anything the caller has, including a bare symbol. Borrowing is also the passing mode for **value types**: a value-type parameter is a read-only borrow whether or not the `'` is written, and a value is **copied** only when it is bound into a fresh slot — an assignment, a new declaration, or a field or return store. The one writable borrow is a `mut` receiver (see [`functions.md`](functions.md) §2.4).

A reference-type **receiver** written `this T` is an implicit borrow: it is never swallowed, and it is what an ordinary method call on a bare symbol supplies. A receiver written `this &T` is a guest receiver, and the call site must then supply a guest source under §2.8.

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

// plain reference-type parameter: taken by hosting access, then moved into a hosting field
Unit setSpare(this Car, engine Engine) mut {
    this.spare = engine
    return Unit()
}

// borrow parameter: read a reference-type object without consuming it or retaining it
Int inspect(this Car, engine 'Engine) {
    return this._value + engine.speed
}
```

The three modes differ in exactly what the caller must have:

```zane
loose Engine()
garage Garage(Engine())

car:inspect(loose)             // legal: a borrow may be rooted in a bare symbol
car!setEngine(loose)           // ILLEGAL: `&Engine` needs a guest source
car!setEngine(garage.engine)   // legal: a field is a guest source
```

Binding a swallowed parameter or a borrow into `&` storage is illegal. A swallowed value is hosted at the call site and a borrow ends with the call, while an `&` field lives with the object that holds it — which may outlive both:

```zane
Unit setEngineWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: a swallowed host is not a guest source
    return Unit()
}

Unit setEngineAlsoWrong(this Car, engine 'Engine) mut {
    this.engine = engine   // ILLEGAL: a borrow cannot be stored
    return Unit()
}
```

This rule preserves uniform call syntax. The call site writes `consume(e)` or `inspect(e)` regardless of the mode; the callee's signature determines what the caller must supply.

> **Story:** [`stories/memory.md`](../stories/memory.md#three-ways-to-hand-over-an-object) — "Three ways to hand over an object".

### 2.10 Value-downstream enforcement (transitive value-only field restriction)

Value types form a closed world of plain value storage. A value-type field may contain primitives (see [`syntax.md`](syntax.md) §2.1) and other value types, but it **MUST NOT** contain a reference type (a `#`-marked type) or an `&`. This rule applies transitively: a value type containing another value type that eventually contains a reference-type or `&` field is also illegal. The same closure forbids a value type from recursing, since a self-reference would need indirection and indirection is a reference.

Here, **downstream** means "through nested value-type fields." The restriction is checked recursively through the full value graph.

Value types are copied and overwritten as ordinary inline values. They have no identity and are not destruction-tracked. If a value could contain a reference-type field, copying it would silently duplicate hosting. If a value could contain an `&`, copying it would silently duplicate a reference without any host accounting for it. Downstream enforcement keeps value copying mechanical, keeps hosting confined to reference types, and — because nothing reachable from a value can be aliased — is what lets a value be shared by snapshot and mutated concurrently under [`concurrency.md`](concurrency.md) §4.

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

Because every storage position is initialized at its declaration and a host slot always holds a live occupant, no reference-type slot and no guest ever holds a null reference (see [`lifetimes.md`](lifetimes.md) §2.4).

---

## 3. Memory Layout

### 3.1 Scope arenas and segmented offsets

Each lexical scope owns an **arena** made from two independent allocation regions:

- The **fixed-size region** stores materialized value-type slots and statically sized reference-type hosts, including the fixed-size handles of dynamically-sized reference types.
- The **dynamic region** stores the resizable backing stores behind handles such as `List` and `String`.

Each region is a separate chain of fixed-size **1 MiB chunks** mapped from the OS on demand. A chunk belongs to exactly one region: fixed-size slots and dynamic backing stores never coexist in the same chunk. A region maps no chunk until its first allocation. When its current chunk cannot satisfy an allocation, the runtime maps another chunk for that region, assigns it the next **chunk id**, and makes it current.

Scopes nest last-in-first-out, and their arenas nest with them: both regions of a scope are unmapped in full the moment the scope drains (§3.2, [`lifetimes.md`](lifetimes.md) §2.1). Arena granularity is an implementation choice, like boolean packing (§3.4) and placement (§3.5) — the compiler may fold several lexical scopes into one arena. What the language fixes is the observable behavior: a scope's memory is released together when that scope drains, and no guest ever resolves into released memory.

```text
one scope arena
──────────────────────────────
fixed-size region   dynamic region
[F1] → [F2]         [D1] → [D2]
```

An ordinary dynamic allocation never straddles a chunk boundary. A dynamic block of at most 1 MiB is wholly contained in one dynamic chunk; if the remaining bytes in the current chunk cannot hold it, allocation continues in a fresh dynamic chunk.

A dynamic block larger than 1 MiB is an **oversized span**: a dedicated contiguous OS mapping made from `block_size / 1 MiB` consecutive dynamic chunks, all belonging exclusively to that block and assigned consecutive chunk ids. Its handle stores the segmented offset of the span's first byte and its size class. After resolving that base, element addressing uses an ordinary byte offset across the contiguous mapping. Every constituent chunk also has a directory entry. Returning an oversized span pushes only its base offset onto the exact-size stack; the complete span remains mapped for reuse until the scope drains.

All chunks draw ids from one chunk directory, so payload storage, dynamic handles, guests, and size-stack entries all use one **`u32` segmented offset**:

```
   u32 segmented offset
  ┌───────────────┬──────────────────────────┐
  │   chunk id    │   in-chunk word offset   │
  │  (high bits)  │       (low bits)         │
  └───────────────┴──────────────────────────┘
```

Allocations are at least 8-byte aligned, so the low bits count 8-byte words: a 1 MiB chunk holds 2¹⁷ words, so **17 low bits** address any slot in a chunk and the remaining **15 high bits** select one of up to 32768 live chunks — a reach of 32 GiB. The chunk directory maps a chunk id to the chunk's native base address, so an address is materialized only at use, as `directory[chunk id] + word offset × 8`: splitting the `u32` is a shift and a mask, and the directory lookup is one load.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-last-table-problem-and-the-segmented-offset) — "The last table problem, and the segmented offset".

### 3.2 Allocation, reuse, and teardown

The fixed-size region is a pure bump allocator: no size classes, no free list, no coalescing. A host has a fixed-size storage slot, so overwriting it destroys the current occupant and initializes the replacement directly in the same slot (§2.2); the overwrite consumes no new space in the fixed-size region. Any dynamic backing stores owned by the destroyed occupant are returned to their exact-size stacks before the replacement becomes live. Nothing in the fixed-size region is reclaimed individually — bytes in a slot that cease to be live before the scope drains remain dead space until teardown.

The dynamic region adds exact-size reuse on top of its bump frontier. Dynamic blocks use power-of-two byte sizes beginning at **128 bytes**. Each scope maintains one LIFO **size stack** for every block size that has become reusable. To allocate a dynamic block of size `S`, the runtime first pops `size_stack[S]`; only when that stack is empty does it bump the dynamic frontier. It never satisfies a request from another size stack and never coalesces neighbouring blocks.

Returning a dynamic block pushes its base segmented offset onto the stack for that exact byte size. The stacks are shared by all dynamic types in the scope: a 128-byte block previously used by a `List<Int64>` may later hold string bytes or another list's elements. An oversized span participates in the same exact-size policy.

When a scope drains — after all its spawned work completes ([`concurrency.md`](concurrency.md) §4.1) — the runtime unmaps its fixed-size and dynamic chunks in bulk, with no per-object teardown pass threaded through the exit. Logical destruction timing is independent of this: a value dies when its host dies or its hosting scope drains ([`lifetimes.md`](lifetimes.md) §2.1); it is the *memory* that is reclaimed together at drain.

> **Story:** [`stories/memory.md`](../stories/memory.md#when-the-free-stacks-fragment-and-the-arena-takes-the-scope) — "When the free stacks fragment, and the arena takes the scope".

### 3.3 Fields are inline; only a containment cycle forces indirection

Fields are laid out in declaration order and stored **inline** in their container, whether the field's type is a value type or a reference type. A statically sized reference-type instance sits inline in its host slot, so value-type slots and reference-type host slots may sit directly beside each other, and an array of either is densely packed. Reference types differ from value types by identity and hosting semantics, not by requiring a separate indirect allocation.

An inline reference-type field is reachable by a guest for the same reason an inline value field is not relocated: the host slot it sits in never moves, so a fixed offset into that slot is a stable address for as long as the host lives (§4.3).

The one case inline layout cannot express is a **containment cycle** — a reference type that reaches itself through hosting fields — which would have no finite size. The compiler stores such a member indirectly instead, as a `u32` segmented offset to storage of its own. This is a placement decision under §3.5, not a language-visible one: the member is an ordinary hosting field, uniform stride still holds because the indirect member has a fixed size, and no boxing type appears in the source (see [`adt.md`](adt.md) §4).

A guest slot (`&T`) always holds a `u32` segmented offset. It never holds an inline payload, because it hosts nothing.

### 3.4 Booleans may be packed

The compiler may pack booleans in structs and arena frames when doing so does not change language semantics.

### 3.5 Placement is the compiler's

Placement is an implementation decision, not a language-visible property. The arena model places every materialized value-type slot and every statically sized reference-type host inline in a scope's fixed-size region, and every resizable backing store in a dynamic region. The compiler may keep an unobservable value in registers or otherwise optimize its physical placement.

A hosted object is constructed **directly in the storage of the host it comes to rest in**. Which host that is, is statically known: hosting changes only by a move, a move-source is only ever a bare symbol ([`lifetimes.md`](lifetimes.md) §1.2) moved in its own declaration block ([`lifetimes.md`](lifetimes.md) §1.3), and every destination is at the same or a higher lexical scope ([`lifetimes.md`](lifetimes.md) §1.4). The compiler therefore reads the whole chain of moves off the declaration block and materializes the object once, in the final host's slot.

A bare symbol that is later moved is thus a **name for the destination's storage from the outset**, not separate storage that is copied out of. In `engine Engine()` followed by `car Car(engine)`, the Engine is constructed directly in the `engine` field inside `car`, and the symbol `engine` names that field. This is why an inline field costs nothing at the move: there was never a second copy of the bytes to reconcile. An object therefore never has to be relocated in order to outlive the scope it was written in.

Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and a guest resolves identically regardless of physical placement (§4), because a hosted object does not move.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-value-world-stays-closed-and-placement-stays-the-compilers) — "The value world stays closed, and placement stays the compiler's".

### 3.6 Handle-typed dynamic reference types have fixed footprint

Dynamically-sized reference types such as `List`, `String`, and similar types are represented as fixed-size **handles**. A handle records the backing store's segmented offset and the metadata needed by the type, such as length and size class. The handle occupies a statically known footprint inline in the fixed-size region; its resizable backing store is a separate allocation in the dynamic region.

A type that contains a handle-typed field therefore stays statically sized:

```zane
type Inventory = #struct {
    items List<Item>;   // fixed-size handle; elements in the dynamic region
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

A block never grows in place across a chunk boundary, and an oversized span is never extended in place: further growth relocates into a doubled oversized span after checking that exact-size stack first. Growth relocates the *elements* of the backing store, which is the one relocation the model still performs — and it is exactly why no guest may be created from a `[]` expression or from any path rooted in one (§2.8).

Dynamic chunks, ordinary power-of-two blocks, and oversized spans begin at cache-line-aligned addresses. Because the minimum block is 128 bytes and every larger block doubles, frontier allocations, reused blocks, and dedicated spans preserve cache-line alignment without mixing backing stores into fixed-size chunks.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-sentinel-that-costs-nothing-and-the-buffer-that-wanted-a-line) — "The sentinel that costs nothing, and the buffer that wanted a line".

### 3.7 A move transfers hosting, not storage

A move transfers hosting into a destination storage position of the **same type** (see [`lifetimes.md`](lifetimes.md) §1). The object itself does not move, and no second copy of it ever existed: the source symbol and the destination slot are two **names for the same storage**, because the object was materialized in the destination's slot when it was constructed (§3.5). A move is therefore a compile-time change in which name is responsible for destruction ([`lifetimes.md`](lifetimes.md) §1.6), not a transfer of bytes between two slots.

- Moving into a fresh declaration or a return slot makes that position the host.
- Moving into an already-initialized host first destroys that host's current occupant, then makes the position host the moved value.

Because a move never relocates an object, it copies no payload bytes regardless of the size of the representation, no dynamic backing store is relocated, and no guest is enumerated, rewritten, or invalidated. It emits no code at all.

> **Story:** [`stories/memory.md`](../stories/memory.md#the-move-that-stopped-relocating) — "The move that stopped relocating".

---

## 4. References

### 4.1 A reference is a segmented offset

A guest holds one **`u32` segmented offset** (§3.1) naming the storage it reaches. It is not a native pointer and not a table index. At half the width of a 64-bit pointer, twice as many references fit in a cache line, and the 32-bit encoding keeps resolution on cheap 32-bit CPU math. A cycle-broken hosting member (§3.3) uses the same encoding.

A borrow (`'T`) is not stored and needs no encoding of its own; an implementation may pass it as a native address, since it cannot escape the call.

There is no per-instance metadata. An instance costs the storage it occupies, and each guest reaching it costs four bytes wherever that guest is stored.

### 4.2 Resolving a reference

Resolving a reference splits the `u32` into a chunk id and a word offset, reads the chunk's base from the chunk directory, and adds the offset. Field access then applies an ordinary field offset:

```zane
dps Float = mainWeapon.dps
```

```text
mainWeapon: &Weapon
│
│ segmented offset
▼
chunk directory
│
▼
fixed-size chunk
│
▼
Weapon storage
│
│ ordinary field offset
▼
Weapon.dps
```

The cost over a direct inline access is one chunk-directory load, which is hot and shared by every reference in the program. Across repeated accesses through the same guest the compiler may resolve the address once and reuse it, because no intervening operation can move the object.

### 4.3 Why references never dangle

A dangling reference would require a guest to outlive the storage it names, or that storage to move underneath it. Four rules together forbid both:

- A hosted object is never relocated. A move transfers hosting without moving bytes (§3.7), and an overwrite constructs the replacement in the same storage (§2.2).
- Storage is allocated in the scope the object comes to rest in (§3.5) and released only when that scope's arena is unmapped (§3.2), so it is never released while a host still names it.
- A guest may be created only from a field access or another guest (§2.8) — never from a bare symbol, which a move can empty, and never from a `[]` expression or a path rooted in one, whose element storage a container may relocate.
- A guest may be created only when the target's host is declared in the same or a higher lexical scope ([`lifetimes.md`](lifetimes.md) §1.1).

A borrow is bounded by a stricter rule still: it cannot be stored or returned (§2.9), so it cannot outlive the call that created it.

> **Story:** [`stories/memory.md`](../stories/memory.md#where-a-guest-may-be-rooted) — "Where a guest may be rooted".

---

## 5. Language Comparisons

### 5.1 Hosting and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single host by default | ✅ | ❌ | ❌ | ✅ |
| Non-hosting guests as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Reference counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Guests remain usable across moves | ✅ moves do not relocate | ❌ | ❌ | ⚠️ only when borrow checking permits the move pattern |
| Host overwrite keeps existing guests valid | ✅ the storage persists | ❌ | ❌ | ⚠️ heavily restricted by borrow checking |

### 5.2 Allocation

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Allocation strategy | per-scope fixed-size and dynamic arenas | runtime-managed | allocator-dependent | allocator-dependent |

> **See also:** [`lifetimes.md`](lifetimes.md) §3 for the lifetime and destruction behavior comparison.

---

## 6. Summary

| Concept | Rule |
|---|---|
| Hosting storage | Reference-typed symbols, fields, and container elements are directly initialized and may later be overwritten |
| Value type | Mutable in place through a borrowed `mut` receiver; storage may also be overwritten freely |
| `&` (guest) | Non-hosting storage holding one reference to a hosted object; may be repointed, copied, stored, and returned, but never hosts |
| `'` (borrow) | Non-hosting, non-escaping access for the duration of one call; may be rooted in anything; **MUST NOT** be stored or returned |
| Place expression | Existing stable storage: a named symbol, a field access of a place, a place-projection subscript of a place, or an `&T`/`'T` parameter |
| Guest source | Only a field access of a place, or an `&T` parameter; a bare symbol, a temporary, a `[]` expression, and any path rooted in a `[]` are all rejected |
| Reference-type parameter | `T` swallows; `&T` is a guest and needs a guest source; `'T` is a borrow and accepts any place |
| Reference-type receiver | `this T` is an implicit borrow; `this &T` is a guest receiver and requires a guest source at the call |
| Value-type parameter | A read-only borrow; caller need not supply a guest source; copied only when bound into a fresh slot |
| Value-downstream enforcement | Value types may contain only primitives and other value types, transitively — never a reference (`#`) or `&` field |
| `&` targets reference types | An `&T` requires `T` to be a reference type; a value is shared by copy or borrow, never by a stored `&` |
| Symbol declaration | Must be directly initialized; no reference-type slot or guest is ever null |
| Field layout | Every field is inline, value or reference alike; only a containment cycle forces a member to be stored indirectly, as a placement decision |
| Move | Transfers hosting at compile time; the object is never relocated, no payload bytes are copied, and no guest is affected |
| Overwrite | Destroys the host's occupant and constructs the replacement in the same storage; existing guests observe the new occupant |
| Addressing | One `u32` segmented-offset directory; 8-byte-aligned offsets reach 32 GiB across up to 32768 1 MiB chunks |
| Dynamic allocation | Power-of-two byte classes beginning at 128 bytes; exact-size stack first, frontier second; blocks above 1 MiB use dedicated contiguous oversized spans |
| Backing-store alignment | Dynamically-sized backing stores (§3.6) are cache-line-aligned; small inline allocations stay 8-byte aligned |
| Per-instance overhead | None; an instance costs the storage it occupies plus four bytes per guest reaching it |

> **See also:** [`lifetimes.md`](lifetimes.md) §4 for the summary of scope, move, and destruction rules.
