# Zane Memory Model

## Overview

Zane's memory model is built on **single ownership** with **refs**, structured as a strict **supervisor tree**. Every object on the heap has exactly one owner at all times. Ownership is the default — the `ref` keyword is the explicit opt-in for non-owning references.

There are no shared pointers, no garbage collector, no reference cycles, and no lifetime annotations. Destruction is deterministic and always propagates strictly downward through the ownership tree.

At the runtime level, memory is a single contiguous region mapped once at program startup, divided into three sections whose boundaries shift as the program runs but never collide. Every allocation — heap objects, list data, stack frames, and local variables — lives inside one of these sections and never outside them.

There is no general-purpose allocator. There is no `malloc`. Each section uses a strategy precisely matched to the access patterns of the data it holds.

---

## Ownership

### 1. Every object has exactly one owner
When an object is created, it is born with a single owning variable. There can never be more than one owner of the same object at any point in time.

### 2. Ownership is the default
Simply declaring a variable makes it the owner of the object assigned to it. There is no keyword for ownership — it is the unmarked, default case. Objects are created by calling a constructor — they cannot be created by copying or assigning from another owning variable.

```zane
Tank tank = Tank(...)          // tank owns this Tank
Tank clone = tank              // compile error — cannot copy or move between owning variables
ref Tank myTank = tank         // ok — myTank is a non-owning reference to tank's object
```

This rule eliminates ambiguity about which variable owns the object. At any point in the code, the owner is always the variable that received the constructor call, or the container/field that received it via ownership transfer (see §Lifetime composition).

### 3. A ref is an explicit non-owning reference
The `ref` keyword creates a non-owning reference to an existing object. A ref does not control the object's lifetime. If the owner is destroyed while a ref exists, the ref becomes null and any dereference is a caught error.

```zane
ref Tank myTank = tanks[0]   // myTank does not own the Tank
```

Refs can be declared as local variables or as class fields. A local ref lives as long as its scope. A ref field lives as long as the containing class instance.

### 4. Ownership is declared in the type

Containers own their elements by default. Elements are stored inline — contiguous in memory, no pointer chase. The `ref` keyword in a type parameter opts out of ownership: the container holds non-owning references to objects owned elsewhere.

```zane
List<Tank>       // the list owns the Tanks — data stored inline, contiguous
List<ref Tank>   // the list holds non-owning refs to Tanks owned elsewhere
```

When a `Tank` is placed into a `List<Tank>`, ownership transfers to the list. When the list is destroyed, all Tanks it contains are destroyed with it. There is no separate transfer keyword — the type declaration is the contract.

For class fields, the same rule applies. A field declared with a class type means the containing class owns that object. The `ref` keyword on a field declares a non-owning reference.

```zane
class World {
    Player player              // World owns this Player
    ref Player spectated       // non-owning reference to a Player owned elsewhere
    List<Tank> tanks           // World owns the list and all Tanks in it
    List<ref Tank> visible     // World owns the list, but the Tanks are owned elsewhere
}
```

Structs cannot hold class fields or ref fields — see struct-downstream enforcement in §Struct and class layout.

### 5. When the owner goes out of scope, the object is destroyed
The object's destructor runs, its memory is returned to the heap, and **all refs to it are immediately nulled** via its anchor. There is no dangling pointer risk — accessing a null ref is a caught error, not undefined behaviour.

### 6. Refs do not extend lifetime
A ref going out of scope does not destroy the object — it destroys the ref itself. Only the owner controls the object's lifetime. A ref to a temporary that has no owning variable is immediately null — the temporary is destroyed at the end of the statement because no owner catches it.

```zane
ref Tank ghost = Tank(...) // Tank is created, but no owner catches it — destroyed immediately
                           // ghost is null from the start — dereferencing it is a caught error
```

### 7. The ownership structure is always a tree
Because only one owner can exist at a time, and a child cannot own its parent, ownership forms a strict tree. There are no cycles. When any node in the tree is destroyed, its entire subtree is destroyed with it.

---

## Lifetime composition

Objects are composed by placing them into owning containers or scopes. Ownership transfers at the point of assignment or insertion — the source variable is consumed and cannot be used again:

```zane
let tanks: List<Tank> = List()
let tank = Tank(...)
tanks.push(tank)           // ownership transfers to the list — tank is consumed
tank.fire()                // compile error — tank was moved

let player = Player(...)
world.player = player      // ownership transfers to World — player is consumed
player.move()              // compile error — player was moved
```

An object can be moved into an owning location exactly once. After the move, the source variable is dead and any use is a compile-time error. There is no implicit copy — ownership transfer is always explicit through assignment or insertion.

For class composition, a field declared with a class type is the owner of that field's value for the class's lifetime. A field declared with `ref` is a non-owning reference to an object owned elsewhere. The compiler enforces that a value is not used after it has been transferred to an owner.

---

## Destruction and the supervisor tree

When an owning variable dies — either by going out of scope or being replaced — the following happens in order:

1. The object's destructor runs (user code).
2. All owned children (class fields, list elements) are destroyed recursively by the same rules.
3. All `ref` fields on the object are unregistered from their targets and the ref objects are freed.
4. If refs to this object exist: the anchor nulls every registered ref — all refs to the object become null immediately. The anchor is then freed.
5. The object's memory is freed.

### Why children-before-parent

Children are destroyed before the parent's own ref cleanup (step 3) and before the parent's own anchor teardown (step 4). This is a strict post-order traversal of the ownership tree for cleanup, with the user destructor running pre-order.

The ordering guarantees two properties:

- **The user destructor sees a fully live subtree.** When step 1 runs for a given object, all of its owned children are still alive. The destructor can access, inspect, or finalize any child. Only after the destructor completes do children begin tearing down.
- **Cleanup proceeds strictly downward.** After the user destructor runs, each child is destroyed recursively (running its own destructor, then its children, and so on). By the time the parent's own anchor and memory are freed (steps 4–5), the entire subtree below it is already gone. No object is ever freed while a descendant still holds a live reference to it.

### Cost of the back-pointer check

At each node during recursive teardown, step 4 checks whether the object has any refs by testing `back_ptr == 0`. Because refs are rare in practice, this branch is almost always not-taken. The branch predictor learns the pattern immediately and the check costs effectively nothing on modern hardware — a single correctly-predicted branch per object in the tree. The total ref-nulling work across an entire subtree is exactly proportional to the number of refs that actually exist, not the number of objects destroyed.

For objects with no ref fields, step 3 has zero runtime cost — the compiler knows statically which fields are refs and emits no cleanup code when there are none.

### Exception: return

If an owned value is returned from a function, the returning scope does not destroy it. Ownership transfers to the caller instead. The object's lifetime extends into whatever scope receives the return value. If the caller discards the return value, destruction happens there.

Destruction order is always deterministic and knowable at compile time. No graph traversal, no cycle detection, no GC pause. For the full destruction protocol pseudocode, see §Destruction protocol.

---

## Error behaviour

Accessing a null ref — one whose owner has been destroyed — is a caught runtime error that terminates the offending branch cleanly. There is no silent undefined behaviour.

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
    list.move_to(new_offset)             // see: §Object move protocol
    free(list.offset, list.current_size)
```

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

Structs form a closed world of value types. A struct may only contain fields that are primitives or other structs — **never a class or a ref**. This is enforced by the compiler and applies transitively: a struct containing a struct containing a class is also an error.

The reason is ownership and copying. Structs are copied by flat `memcpy` — there is no destructor, no anchor, no heap interaction. If a struct could contain a class field, copying the struct would silently duplicate a strong reference, violating single ownership. If a struct could contain a ref field, copying would duplicate a weak reference without registering the copy in the target's `weak_ref_stack`. Struct-downstream enforcement makes both impossible at the type level.

Classes, by contrast, may contain struct fields, class fields (owning), and ref fields (non-owning).

```
struct Vec2  { x: f32, y: f32 }          // ok — only primitives
struct Rect  { pos: Vec2, size: Vec2 }   // ok — only structs
struct Bad   { name: String }            // error — String is a class
struct Bad2  { ref target: Player }      // error — ref in a struct

class Player { pos: Vec2, hp: i32 }      // ok — struct fields in a class
class World  { players: List<Player> }   // ok — class fields in a class
class Unit   { ref target: Player }      // ok — ref field in a class
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

## Anchors and refs

A `ref` does not point at an object directly. It points through an **anchor** — a small separately-allocated object with a fixed heap address that tracks the target object's current location. The indirection chain is:

```
ref_anchor (stack or heap)  →  ref object (heap)  →  target anchor  →  object
```

The variable that holds a `ref` — whether a stack local or a class field — is its **ref_anchor**. The ref object keeps a back-pointer to this ref_anchor so the system can null it when the target is destroyed.

### Anchors

An **anchor** is a small heap-allocated object with a fixed address that never changes. It holds the current heap-relative offset of the object it represents. When an object moves, only the anchor's offset is updated — all refs to that object remain valid automatically.

```
anchor: {
    heapoffset:    u32                  // heap-relative offset of the object's current location
    weak_ref_stack: Stack<*ref_anchor>  // absolute addresses of the ref_anchors of all live refs
}
```

Anchors are created lazily — only when the first `ref` to an object is made (see the back-pointer section above for the 0-sentinel guarantee).

### Refs

A `ref` is a heap-allocated object that holds a pointer to the target object's anchor, a back-pointer to its own `ref_anchor`, and a `stack_index` for O(1) unregistration. The `ref_anchor` is the variable that holds the ref — either a stack local or a class field. It stores a `u32` heap-relative offset to the ref object.

```
ref object:   { target_anchor: *anchor, back_ptr: *ref_anchor, stack_index: u32 }
ref_anchor:   { heapoffset: u32 }   // stack variable or class field — no weak_ref_stack,
                                    // because refs cannot be ref'd
```

The `stack_index` records the ref's position in its target anchor's `weak_ref_stack`. This enables O(1) unregistration when the ref is destroyed.

**All pointers to anchors and ref_anchors are absolute addresses.** Anchors never move, so a pointer to one is permanently valid. Ref_anchors on the stack never move. Ref_anchors embedded in class fields can move when the containing class relocates — this case is handled by the ref_anchor move protocol (see below).

Dereference resolves the target through the anchor:

```
ref Tank myTank = tanks[0]

dereference:
    ref_obj = heap_base + myTank.heapoffset
    object  = heap_base + ref_obj.target_anchor.heapoffset
```

### Leaf-only registration

When a ref is created, its `ref_anchor`'s absolute address is registered in the `weak_ref_stack` of only the **leaf** object's anchor — the object the ref directly points to. It does not register with any parent or ancestor anchors.

```
ref Player myPlayer = world.players[0]
// registers &myPlayer only in: player_anchor.weak_ref_stack
```

When an ancestor is destroyed, destruction recurses through the ownership tree. Each child's destruction checks its own `back_ptr`. If non-zero, that child's anchor iterates its `weak_ref_stack` and nulls all refs. The total ref-nulling work across the entire subtree is exactly proportional to the number of refs that exist.

This keeps ref creation and destruction O(1) — one registration, one unregistration — at the cost of one `back_ptr == 0` branch per child during destruction. Since refs are rare, `back_ptr` is almost always 0, the branch predictor learns this pattern immediately, and the check is effectively free on modern hardware.

### Ref lifetime

A ref is owned by its ref_anchor — either a stack variable or a class field. When the ref_anchor is destroyed — because a local goes out of scope or a containing class is destroyed — the ref object is destroyed and its `ref_anchor` address is removed from the target anchor's `weak_ref_stack` using swap-with-last-and-pop, indexed by the ref's stored `stack_index` — O(1). When a ref is swapped to a new position during another ref's unregistration, the swapped ref's `stack_index` is updated to reflect its new position.

If a `ref` is returned from a function, the compiler creates a new ref to the same target in the caller's scope. The original ref is destroyed at the end of the returning scope as normal.

Refs as class fields are destroyed as part of the containing class's destruction protocol — after owned children are destroyed, all ref fields are unregistered from their targets and the ref objects are freed.

### Object move protocol

When an object needs to relocate to a new heap slot:

```
1. read:   anchor = object.anchor_ptr              // follow absolute back-pointer at old address
2. write:  anchor.heapoffset = new_offset          // update anchor before moving
3. copy:   memcpy(new_address, old_address, sizeof(T))
4. free:   free_stacks[sizeof(T)].push(old_offset)
```

Step 2 must happen before step 3. All existing refs are unaffected — they follow `target_anchor.heapoffset` which now points to the new address.

Because owned fields are inlined, all children in the subtree physically move with the parent. The compiler must also fixup any child that has an active anchor or ref fields:

```
for each inlined child (recursively, statically known from type layout):
    if child.back_ptr != 0:
        child_anchor = child.back_ptr
        child_anchor.heapoffset = new_offset_of_child   // update child's anchor
    for each ref field in child:
        ref_obj = heap_base + ref_field.heapoffset
        ref_obj.back_ptr = &new_address_of_child.ref_field
        anchor = ref_obj.target_anchor
        anchor.weak_ref_stack[ref_obj.stack_index] = &new_address_of_child.ref_field
```

The same applies to the moved object's own ref fields:

```
for each ref field in moved object:
    ref_obj = heap_base + ref_field.heapoffset
    ref_obj.back_ptr = &new_address.ref_field           // update ref's back-pointer
    anchor = ref_obj.target_anchor
    anchor.weak_ref_stack[ref_obj.stack_index] = &new_address.ref_field  // update target's entry
```

The full set of fixups is statically known from the type layout — the compiler emits a flat sequence of writes at the move site. No runtime discovery, no object-side move logic.

### Ref move protocol

When a ref object needs to relocate (e.g. inside a `List<ref T>` that grows):

```
1. read:   ref_anchor = ref_obj.back_ptr           // follow absolute back-pointer at old address
2. write:  ref_anchor.heapoffset = new_offset      // update ref_anchor before moving
3. copy:   memcpy(new_address, old_address, sizeof(ref))
4. free:   free_stacks[sizeof(ref)].push(old_offset)
```

The ref_anchor is updated in step 2. The target anchor is not touched — only the ref's location changed, not the target's.

### Destruction protocol

When an object is destroyed:

```
1. destroy all owned children recursively (same protocol)

2. for each ref field on this object:
       ref_obj = heap_base + ref_field.heapoffset
       anchor  = ref_obj.target_anchor
       // unregister using swap-with-last-and-pop:
       last_index = anchor.weak_ref_stack.len - 1
       if ref_obj.stack_index != last_index:
           swapped_ref_anchor_ptr = anchor.weak_ref_stack[last_index]
           anchor.weak_ref_stack[ref_obj.stack_index] = swapped_ref_anchor_ptr
           swapped_ref_obj = heap_base + swapped_ref_anchor_ptr.heapoffset
           swapped_ref_obj.stack_index = ref_obj.stack_index
       anchor.weak_ref_stack.pop()
       free ref object slot

3. if object.back_ptr == 0:
       free object slot

   else:
       anchor = object.back_ptr
       for each ref_anchor_ptr in anchor.weak_ref_stack:
           ref_anchor_ptr.heapoffset = 0xFFFFFFFF    // null sentinel — ref is now dead
       free anchor slot
       free object slot
```

Children are destroyed before the parent's own ref fields are cleaned up and before the parent's own anchor teardown. At each level, the `back_ptr == 0` check determines whether any ref-nulling is needed. Since refs are rare, this branch is almost always not-taken and costs effectively nothing on modern hardware. The compiler knows statically which fields are refs, so step 2 has zero cost for objects with no ref fields.

When a ref local goes out of scope:

```
ref_obj = heap_base + ref_anchor.heapoffset
anchor  = ref_obj.target_anchor

// O(1) swap-with-last-and-pop using stored stack_index:
last_index = anchor.weak_ref_stack.len - 1
if ref_obj.stack_index != last_index:
    swapped_ref_anchor_ptr = anchor.weak_ref_stack[last_index]
    anchor.weak_ref_stack[ref_obj.stack_index] = swapped_ref_anchor_ptr
    // update the swapped ref's stack_index:
    swapped_ref_obj = heap_base + swapped_ref_anchor_ptr.heapoffset
    swapped_ref_obj.stack_index = ref_obj.stack_index
anchor.weak_ref_stack.pop()

free ref object slot
// ref_anchor (stack variable) is reclaimed with the stack frame
```

---

## Inline list storage

`List<T>` stores element data **inline** — contiguous in the list's heap allocation, no pointer chase. This is the default and only owning mode. `List<ref T>` holds non-owning references to objects owned elsewhere.

```
List<Tank>      →  [ Tank | Tank | Tank ]   Tank data inline, no pointer chase
List<ref Tank>  →  [ ref, ref, ref ]        each ref → Tank owned elsewhere
```

Each element within a `List<T>` has its own anchor holding the element's current absolute heap offset — updated whenever the list moves. When the list grows and relocates, all element anchors are updated in the same pass. Any `ref` to a list element dereferences directly through the element's own anchor: `heap_base + element_anchor.heapoffset`. No addition of parent and child offsets is needed.

Because elements are stored inline and never individually freed, `List<T>` does not support removal from the middle. Elements can be appended and removed from the end only. Removing from the end destroys the element using the standard destruction protocol — if the element has a non-zero back-pointer, its anchor's `weak_ref_stack` is iterated to null all refs, the anchor is freed, then the element slot is reclaimed. If ordered iteration with arbitrary removal is needed, a separate index array can impose an access order without disturbing element addresses.

#### Nesting

Inline storage composes naturally through nesting:

```
List<Tank>
  → Tank data inline in the list
  → no pointer chase to reach a Tank

List<List<Tank>>
  → inner List headers inline in the outer list
  → Tank data inline in each inner list
  → the entire structure is contiguous on the heap
```

---

## Benefits

**No fragmentation.** Every freed slot returns to a free stack indexed by its exact size. The next allocation of that size reuses it immediately. There are no unusable holes.

**Unified heap.** Class instances, list data, and anchors share the same heap and the same free stacks. A freed object slot of any size is reusable by any other heap allocation of the same size. There is no over-allocation to one type at the expense of another.

**O(1) allocation and deallocation always.** Allocation is a free stack pop or a frontier bump. Deallocation is a free stack push. Neither depends on heap state, fragmentation, or free order.

**In-place list growth.** Lists that are at the frontier grow without copying. This is the common case for lists allocated in sequence.

**Deterministic destruction.** Strong references destroy objects the instant they go out of scope. The freed slot is immediately available for reuse. No deferred cleanup, no GC pause.

**Random free order is free.** Deallocation is always a single push to a size-indexed free stack. There is no coalescing, no adjacency check, no cost difference between freeing in order or at random.

**Move safety without scanning.** When an object moves, its anchor's `heapoffset` is updated in one write. Because owned fields are inlined, all children in the subtree move with the parent — the compiler emits fixups for any child anchors and ref registrations from the statically known type layout. When a ref object moves, exactly one write is needed — updating the ref_anchor's `heapoffset` via the ref's back-pointer. No heap scanning is required in either case.

**O(1) bulk nulling on destruction.** When an object is destroyed, all refs to it are nulled in one pass over the anchor's `weak_ref_stack`. No heap scanning, no reference counting.

**Cache-friendly inline storage.** `List<T>` stores elements inline, eliminating pointer chasing entirely. Iterating elements is a pure linear scan through contiguous memory.

**Statically known layout.** Every class size, every possible list slot size, and every struct layout is known at compile time. The free stack table is a static array. The compiler emits direct arithmetic for all allocations.

---

## Downsides

**Fixed total region size must be chosen upfront.** The mmap reservation is fixed at startup. If the heap and stack together exhaust the free region the program terminates. Sizing the reservation is a deployment concern.

**Anchor overhead per ref.** Objects that are never ref'd pay zero anchor overhead — the back-pointer slot is 8 bytes initialised to 0 and the anchor is never allocated. Objects that are ref'd pay one anchor allocation (O(1)) plus one ref object allocation per `ref` variable. Both go through the same free stack as any other heap allocation.

**Lists cannot remove from the middle.** Inline storage gives stable element addresses, which means arbitrary removal would invalidate refs. Only append and end-removal are supported.

**Same-size slot contention.** All types of the same byte size share one free stack. A program that heavily allocates and frees objects of many different types with the same size will see those slots interleaved in memory. Per-type sequential access patterns lose their locality. This is a tradeoff against the flexibility of a unified heap.

**Stack boolean packing has read-modify-write cost on writes.** Writing a boolean local requires reading the packed byte, masking the bit, and writing it back. On modern hardware this is negligible, but it is not a single-instruction store.

---

## Comparison

### Ownership model

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner | ✅ | ✅ | ❌ | ✅ |
| Refs nulled on destroy | ✅ | ❌ | via `weak_ptr` | ❌ |
| Ownership by default | ✅ | ❌ | ❌ | ✅ |
| Non-ownership opt-in (`ref`) | ✅ | ❌ | ❌ | ✅ (`&`) |
| Inline element storage | Default | ❌ | ❌ | manual |
| Ownership cycles possible | ❌ | ❌ | ✅ | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Ref counting | ❌ | ❌ | ✅ | via `Rc`/`Arc` |
| Garbage collector | ❌ | ❌ | ❌ | ❌ |
| Move safety (anchor) | ✅ | ❌ | ❌ | compile-time |
| Refs as class fields | ✅ | ✅ | ✅ | ✅ |

### Memory layout

| Property | Zane | GC language (JVM, Go) | Rust | C/C++ |
|---|---|---|---|---|
| Fragmentation | None | Managed by GC | Allocator-dependent | Allocator-dependent |
| GC pauses | None | Yes | None | None |
| Allocation cost | O(1) always | Fast (bump alloc + GC) | Allocator-dependent | Allocator overhead |
| Deallocation cost | O(1) always | Deferred to GC | Allocator-dependent | Coalescing overhead |
| Random free order cost | Same as sequential | Deferred | Same as sequential | Higher (coalescing) |
| Destruction timing | Deterministic | Non-deterministic | Deterministic | Manual / RAII |
| Dangling pointer risk | None (refs via anchors) | None | None (compile-time) | Yes |
| Move safety | O(1) anchor update | GC-managed | compile-time (Pin) | Manual |
| Ref nulling on destroy | O(1) via anchor stack | N/A | N/A | N/A |
| Lifetime annotations | None | None | Required | None |
| Struct/class layout | Declaration order, auto bool-pack | JVM-managed | Repr-controlled | Manual / compiler |
| Bool packing | Automatic | No | Manual (`bitflags`) | Manual (bitfields) |
| Inline list storage | Default | No | Manual (`Vec<T>` inline) | Manual |
| Per-type memory isolation | No (shared free stacks) | No | No | No |
