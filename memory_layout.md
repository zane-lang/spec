# Zane Memory Layout

## Overview

Zane's memory layout is a single contiguous region of memory mapped once at program startup. It is divided into three sections whose boundaries shift as the program runs but never collide. Every allocation in the runtime — heap objects, list data, stack frames, and local variables — lives inside one of these sections and never outside them.

There is no general-purpose allocator. There is no `malloc`. Each section uses a strategy precisely matched to the access patterns of the data it holds.

---

## Top-level layout

```
[ HEAP (grows →) ][ ... FREE ... ][ STACK (← grows) ]
 ↑                                                   ↑
 low address                             high address
```

All three regions are carved from a single `mmap` call at startup. The free region between the heap and the stack acts as a natural buffer — the heap grows upward into it, the stack grows downward into it. Neither can silently overwrite the other. If they meet, the program terminates with an out-of-memory error.

---

## Sections

### Heap

The heap holds all class instances, all list data, and all anchors. It is a single growing region with a frontier pointer. Allocating new memory advances the frontier. Freed memory is returned to a **size-indexed free stack table** and reused before the frontier is advanced again.

#### Free stack table

The free stack table maps allocation sizes to stacks of free addresses:

```
free_stacks: {
    8:   [ 0x1020, 0x1040, ... ]
    16:  [ 0x2080, ... ]
    24:  [ 0x3010, ... ]
    32:  [ ... ]
    ...
}
```

Every freed slot — whether it was a class instance, list data, or an anchor — is pushed onto the stack for its exact byte size. Every new allocation first checks whether a free slot of the right size exists before advancing the frontier. All heap allocations share the same free stacks. A freed 16-byte `Bullet` slot is immediately reusable by any 16-byte list allocation, and vice versa.

Because all class sizes and all list slot sizes are statically known, the compiler generates the free stack table at compile time as a static array of stacks indexed by size. Lookup is a direct array index, not a hash probe.

All allocation sizes are rounded up to the nearest multiple of 8 bytes before indexing. This guarantees every allocation is naturally aligned for any field it could contain, and ensures freed slots are always reusable by any allocation of the same declared size. The maximum waste per object is 7 bytes. The size class index is computed as:

```
size_class(size):
    rounded = (size + 7) & ~7     // round up to next multiple of 8
    return (rounded / 8) - 1      // 8→0, 16→1, 24→2, 32→3, ...
```

This is a subtract and a shift — a single array index with no branching, no hashing, and no metadata lookup. The compiler emits the already-rounded size for every class and list slot at compile time, so `size_class` is evaluated statically and never appears in the generated code.

All addresses stored in the free stack table and in anchors are **heap-relative offsets** — unsigned 32-bit integers representing the byte distance from the heap base address. A heap-relative offset can address up to 4 GB of heap space. The absolute address of any heap value is always `heap_base + offset`.

#### Allocation

```
function alloc(size):
    if free_stacks[size] is not empty:
        return free_stacks[size].pop()    // O(1) reuse of freed slot
    else:
        addr = frontier
        frontier += size
        return addr                       // O(1) frontier bump
```

#### Deallocation

```
function free(offset, size):
    free_stacks[size].push(offset)        // O(1) always, regardless of free order
```

No coalescing. No adjacency checks. No metadata writes into the freed slot.

#### List allocation and growth

List data lives directly on the heap alongside class instances. A list slot is simply an allocation of `sizeof(T) * capacity` bytes. When a list needs to grow:

**Case 1 — in-place growth.** If the list's end address equals the heap frontier, the list is the most recently allocated object. Growing simply advances the frontier by the extra bytes needed. No copy, no move, no address change.

**Case 2 — blocked growth.** If something was allocated after the list, growing requires moving:

```
function grow(list, new_size):
    new_offset = alloc(new_size)          // free stack or frontier
    list.move_to(new_offset)             // see: move protocol
    free(list.offset, list.current_size)
```

The move protocol is described below under Anchors.

---

### Stack

Holds stack frames for all function calls. The stack grows downward from the high end of the region. Frame allocation is a pointer decrement; frame deallocation is a pointer restore. This compiles to native RSP-relative addressing — no separate software stack pointer is maintained at runtime.

Each frame is laid out by the compiler at compile time. Boolean locals are packed into bits — up to 8 booleans per byte — at the end of each frame, after all other locals. The programmer observes no difference; the compiler emits bit mask operations transparently on every boolean read and write.

---

## Struct and class layout

In Zane, **structs** are value types — they live on the stack or are inlined into the containing struct or class. **Classes** are reference types — they live on the heap. Both follow the same field layout rules.

Fields are laid out in declaration order. The programmer controls field order and is responsible for minimising padding where it matters. Each field is aligned to its own natural size: `i8`/`bool` to 1 byte, `i16` to 2 bytes, `i32`/`f32` to 4 bytes, `i64`/`f64`/pointers to 8 bytes. The total size of a struct or class is rounded up to a multiple of its largest field's alignment, so that arrays of the type remain correctly aligned.

Boolean fields are grouped and packed into bits at the **end** of the struct or class, after all other user-declared fields. The compiler does this automatically and invisibly. Accessing a boolean field emits a read-mask; writing emits a read-modify-write. If a struct or class has no boolean fields, the layout is completely untouched by bool-packing logic.

#### Struct-downstream enforcement

Structs form a closed world of value types. A struct may only contain fields that are primitives or other structs — **never a class**. This is enforced by the compiler and applies transitively: a struct containing a struct containing a class is also an error.

The reason is ownership and copying. Structs are copied by flat `memcpy` — there is no destructor, no anchor, no heap interaction. If a struct could contain a class field, copying the struct would silently duplicate a strong reference, violating single ownership. Struct-downstream enforcement makes this impossible at the type level.

Classes, by contrast, may contain both struct fields and class fields. A class field is a strong ownership relationship — the containing class owns the field for its lifetime.

```
struct Vec2  { x: f32, y: f32 }          // ok — only primitives
struct Rect  { pos: Vec2, size: Vec2 }   // ok — only structs
struct Bad   { name: String }            // error — String is a class

class Player { pos: Vec2, hp: i32 }      // ok — struct fields in a class
class World  { players: List<Player> }   // ok — class fields in a class
```

```
struct Player { alive: bool, active: bool, health: i32, score: i64 }
class Entity  { visible: bool, health: i32, position: f64 }

// Player in memory (stack):
// [score: 8][health: 4][alive|active|......: 1][pad: 3]
// = 16 bytes

// Entity in memory (heap, excluding anchor back-pointer):
// [position: 8][health: 4][visible|.......: 1][pad: 3]
// = 16 bytes
```

#### Anchor back-pointer

Every class instance on the heap carries one additional piece of compiler-managed metadata: an **8-byte absolute address pointing to the object's anchor**. This field is appended after all user-declared fields (including packed bools) and is invisible at the language level — the programmer never declares or accesses it. It is not part of `sizeof(T)` as seen by the programmer.

The back-pointer must be an absolute address because the anchor is the fixed point of the entire reference system. Using a heap-relative offset to locate an anchor would require a stable base to resolve it first — which is precisely the problem anchors exist to solve. Absolute addresses are the only representation that remains valid and directly usable regardless of where in memory the object currently lives.

The back-pointer is initialised to **0** at object creation (lazy anchor allocation). 0 is a safe sentinel because the Zane heap is a region returned by `mmap`, which the OS always maps at a non-zero address. The lowest possible anchor address is `heap_base + sizeof(first_allocation)` — always a large non-zero value on any 64-bit system. 0 is the null pointer in virtual address space and is never mapped to any process, so there is no collision between the sentinel and any real anchor address.

```
back-ptr == 0    →  no anchor exists yet  (object never weakly referenced)
back-ptr != 0    →  absolute address of the anchor  (guaranteed non-zero)
```

Since the back-pointer is 8 bytes and has 8-byte alignment, it frequently slots into end padding that would exist anyway for structs whose size is already a multiple of 8, costing zero extra bytes. When the struct is not a multiple of 8, alignment padding is inserted before the back-pointer as needed, and the total runtime size is rounded up to the next multiple of 8.

```
// Entity runtime layout (heap):
// [position: 8][health: 4][visible|.......: 1][pad: 3][anchor_ptr: 8]
// = 24 bytes runtime, 16 bytes as seen by the programmer
```

Only class instances carry an anchor back-pointer. Structs stored on the stack or inlined as value types do not, because they cannot be individually heap-allocated, independently moved, or weakly referenced.

---

## Anchors

Every heap-allocated object — every class instance and every list — is assigned an **anchor** at the moment of creation. The anchor is a small separately-allocated heap object with a fixed address that never changes for the lifetime of the object it represents. The anchor is the object's persistent identity.

The object itself may move — when a list relocates to a larger slot, for example — but the anchor stays at its original address. All weak references refer to objects through their anchor, so moves are invisible to the rest of the program.

```
anchor: {
    offset:         u32           // heap-relative offset of the object's current location
    weak_ref_stack: List<*anchor> // absolute addresses of registered weak ref anchors
}
```

**All references to anchors are absolute addresses.** The anchor's address never changes, so an absolute pointer to it is permanently valid — no base address is needed to resolve it, and no update is ever required when objects move. This is the fundamental property that makes anchors useful: they are the one thing in the system that can be pointed to absolutely. By contrast, the `offset` field *inside* the anchor is heap-relative, because it points at the object which can move.

The anchor for a standalone class instance holds the instance's current heap offset. The anchor for a `List<store T>` holds the list's current base offset. Each element within a `List<store T>` has its own anchor holding the element's byte offset relative to the list's base — not an absolute heap offset.

#### Anchor lifetime

An anchor is allocated when its object is created and freed when its object is destroyed. The two always live and die together. There is no lazy initialisation — every object has an anchor from birth, regardless of whether any weak references to it are ever created. This keeps the model uniform and eliminates null checks on the anchor pointer.

#### Weak references

A weak reference does not point at an object directly. It holds a `Stack<*anchor>` — a fixed-length sequence of absolute anchor addresses representing the chain of class-typed values traversed to reach the target. The length of the stack equals the number of class-typed levels in the access chain, which is always statically known from the type at the point the weak ref is created. The compiler emits the stack as an inline fixed-length array — no heap allocation.

The first entry is always the outermost heap-allocated object, whose anchor offset is heap-relative. Every subsequent entry is a nested class value, whose anchor offset is relative to its parent's base address.

```
weak_ref = { anchors: Stack<*anchor> }  // length statically known from type

dereference:
    addr = heap_base + anchors[0].offset     // outermost — heap-relative
    for i in 1..anchors.len:
        addr = addr + anchors[i].offset      // each nested level — parent-relative
    return addr
```

The depth of the stack is determined by the number of class-typed levels crossed in the access expression, not by the number of generic type parameters. Struct fields are value types and contribute no anchor to the chain — only class instances do.

```
class Player { age: Int }
class World  { players: List<Player> }

world: World
ref = world.players[0]
// anchors = [ &world_anchor, &list_anchor, &player_anchor ]  — length 3

struct Vec2 { x: f32, y: f32 }
class Line  { start: Vec2, end: Vec2 }

line: Line
ref = line.start          // Vec2 is a struct — no anchor, not weakly referenceable
ref = line               // anchors = [ &line_anchor ]  — length 1
```

All entries in the stack are absolute anchor addresses. The dereference requires no base-address arithmetic to locate any anchor — each is followed directly. Only resolving the final object address at each level requires adding the parent base, which is computed as part of the fold.

#### Weak ref registration

When a weak reference is created, it registers its own anchor's absolute address in the `weak_ref_stack` of **every** anchor in its stack. This means destroying any class-typed object in the access chain — not just the leaf — will null the weak ref.

When a weak reference is destroyed, it removes itself from every anchor stack it registered with. Removal is O(1) via swap-with-last-and-pop.

#### Move protocol

When an object needs to relocate to a new heap slot:

```
1. read:   anchor = object.anchor_ptr              // follow absolute back-pointer at old address
2. write:  anchor.offset = new_offset              // update anchor before moving
3. copy:   memcpy(new_address, old_address, sizeof(T)) // move data
4. free:   free_stacks[sizeof(T)].push(old_offset) // release old slot
```

Step 2 must happen before step 3. The back-pointer is read at the old address; after the copy the old address is considered freed. Updating the anchor first ensures it is valid for exactly one read before the object is gone. All existing weak refs are unaffected — they hold the anchor's absolute address directly and follow it to the new location.

#### Destruction protocol

When an object is destroyed:

```
1. for each anchor_ptr in anchor.weak_ref_stack:
       anchor_ptr.offset = NULL                    // null the weak ref via its absolute anchor address
2. free anchor slot
3. free object slot
```

All weak references to the object are nulled in one pass. No heap-base arithmetic is needed to locate the weak ref anchors — they are followed directly by their absolute addresses. Any subsequent dereference of a nulled weak ref is a caught runtime error.

---

## `List<store T>`

The `store` qualifier in a type parameter changes where the element data lives. `List<T>` holds strong references to heap-allocated `T` instances. `List<store T>` holds the `T` data **inline in the list slot itself** — the list is the storage.

```
List<Tank>        →  [ ptr, ptr, ptr, ptr ]   each ptr → Tank on heap
List<store Tank>  →  [ Tank | Tank | Tank ]   Tank data inline, no pointer chase
```

The list itself has one anchor tracking its current base offset. Each element within a `List<store T>` has its own anchor tracking its byte offset from the list base. When the list grows and moves, only the list anchor is updated — all element anchors hold relative offsets and remain valid.

Because elements are stored inline and never individually freed, `List<store T>` does not support removal from the middle. Elements can be appended and removed from the end only. Removing from the end frees the element's anchor and nulls any weak refs to that element via the element anchor's weak ref stack. If ordered iteration with arbitrary removal is needed, a separate index array can impose an access order without disturbing element addresses.

Structs were always `store` semantics — value types stored inline wherever they appear. `List<store Tank>` is the same intuition extended explicitly to class types.

#### `store` is recursive

The `store` qualifier applies to each type parameter independently. Nesting `store` composes inline storage at every level:

```
List<store Tank>
  → Tank data stored inline in the list slot
  → no pointer chase to reach a Tank

List<store List<store Tank>>
  → outer list stores inner List headers inline
  → each inner list stores Tank data inline
  → the entire structure lives contiguously on the heap, growing as one unit

List<store List<Tank>>
  → outer list stores inner List headers inline
  → each inner list holds strong references to heap-allocated Tanks
  → Tanks live on the heap separately, accessed via pointer
```

The `store` qualifier on the outer type parameter only controls whether that level's data is inline. What the inner type does with its own elements is determined by its own type parameter independently.

---

## Benefits

**No fragmentation.** Every freed slot returns to a free stack indexed by its exact size. The next allocation of that size reuses it immediately. There are no unusable holes.

**Unified heap.** Class instances, list data, and anchors share the same heap and the same free stacks. A freed object slot of any size is reusable by any other heap allocation of the same size. There is no over-allocation to one type at the expense of another.

**O(1) allocation and deallocation always.** Allocation is a free stack pop or a frontier bump. Deallocation is a free stack push. Neither depends on heap state, fragmentation, or free order.

**In-place list growth.** Lists that are at the frontier grow without copying. This is the common case for lists allocated in sequence.

**Deterministic destruction.** Strong references destroy objects the instant they go out of scope. The freed slot is immediately available for reuse. No deferred cleanup, no GC pause.

**Random free order is free.** Deallocation is always a single push to a size-indexed free stack. There is no coalescing, no adjacency check, no cost difference between freeing in order or at random.

**Move safety without scanning.** When an object moves, exactly one write is needed — updating the anchor's offset. No weak refs need to be found or updated. The anchor is the single point of indirection that makes this possible.

**O(1) bulk nulling on destruction.** When an object is destroyed, all weak refs to it are nulled in one pass over the anchor's weak ref stack. No heap scanning, no reference counting.

**Cache-friendly inline storage.** `List<store T>` eliminates pointer chasing entirely. Iterating elements is a pure linear scan through contiguous memory.

**Statically known layout.** Every class size, every possible list slot size, and every struct layout is known at compile time. The free stack table is a static array. The compiler emits direct arithmetic for all allocations.

---

## Downsides

**Fixed total region size must be chosen upfront.** The mmap reservation is fixed at startup. If the heap and stack together exhaust the free region the program terminates. Sizing the reservation is a deployment concern.

**Anchor overhead per object.** Every heap-allocated object carries a 4-byte back-pointer in its runtime layout and a separately allocated anchor on the heap. The back-pointer frequently occupies existing end-padding bytes at no extra cost. The anchor allocation is O(1) and goes through the same free stack as any other allocation.

**`List<store T>` cannot remove from the middle.** Inline storage gives stable element addresses, which means arbitrary removal would invalidate weak refs. Only append and end-removal are supported.

**Same-size slot contention.** All types of the same byte size share one free stack. A program that heavily allocates and frees objects of many different types with the same size will see those slots interleaved in memory. Per-type sequential access patterns lose their locality. This is a tradeoff against the flexibility of a unified heap.

**Stack boolean packing has read-modify-write cost on writes.** Writing a boolean local requires reading the packed byte, masking the bit, and writing it back. On modern hardware this is negligible, but it is not a single-instruction store.

---

## Comparison

| Property | Zane | GC language (JVM, Go) | Rust | C/C++ |
|---|---|---|---|---|
| Fragmentation | None | Managed by GC | Allocator-dependent | Allocator-dependent |
| GC pauses | None | Yes | None | None |
| Allocation cost | O(1) always | Fast (bump alloc + GC) | Allocator-dependent | Allocator overhead |
| Deallocation cost | O(1) always | Deferred to GC | Allocator-dependent | Coalescing overhead |
| Random free order cost | Same as sequential | Deferred | Same as sequential | Higher (coalescing) |
| Destruction timing | Deterministic | Non-deterministic | Deterministic | Manual / RAII |
| Dangling pointer risk | None (weak refs via anchors) | None | None (compile-time) | Yes |
| Move safety | O(1) anchor update | GC-managed | compile-time (Pin) | Manual |
| Weak ref nulling on destroy | O(1) via anchor stack | N/A | N/A | N/A |
| Lifetime annotations | None | None | Required | None |
| Struct/class layout | Declaration order, auto bool-pack | JVM-managed | Repr-controlled | Manual / compiler |
| Bool packing | Automatic | No | Manual (`bitflags`) | Manual (bitfields) |
| Inline list storage | `List<store T>` | No | Manual (`Vec<T>` inline) | Manual |
| Per-type memory isolation | No (shared free stacks) | No | No | No |
