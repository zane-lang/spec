# The Zane Pointer

## Overview

The Zane pointer is the core memory model of the Zane programming language. It enforces **single ownership** with **refs**, structured as a strict **supervisor tree**. Every object on the heap has exactly one owner at all times. Ownership is the default — the `ref` keyword is the explicit opt-in for non-owning references.

There are no shared pointers, no garbage collector, no reference cycles, and no lifetime annotations. Destruction is deterministic and always propagates strictly downward through the ownership tree.

---

## Rules

### 1. Every object has exactly one owner
When an object is created, it is born with a single owning variable. There can never be more than one owner of the same object at any point in time.

### 2. Ownership is the default
Simply declaring a variable makes it the owner of the object assigned to it. There is no keyword for ownership — it is the unmarked, default case. Objects are created by calling a constructor — they cannot be created by copying or assigning from another owning variable.

```zane
Tank tank = Tank(...)          // tank owns this Tank
Tank clone = tank              // compile error — cannot copy or move between owning variables
ref Tank myTank = tank         // ok — myTank is a non-owning reference to tank's object
```

This rule eliminates ambiguity about which variable owns the object. At any point in the code, the owner is always the variable that received the constructor call, or the container/field that received it via ownership transfer (see Lifetime composition).

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

Structs cannot hold class fields or ref fields — see struct-downstream enforcement in `memory_layout.md`.

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

## Refs and anchors

A `ref` does not point at an object directly. It points through an **anchor** — a small separately-allocated object with a fixed heap address that tracks the target object's current location. The indirection chain is:

```
ref_anchor (stack or heap)  →  ref object (heap)  →  target anchor  →  object
```

The variable that holds a `ref` — whether a stack local or a class field — is its **ref_anchor**. The ref object keeps a back-pointer to this ref_anchor so the system can null it when the target is destroyed.

Key properties:

- **Lazy allocation.** An anchor is created only when the first ref to an object is made. Objects with no refs have no anchor and no overhead beyond a zeroed back-pointer slot.
- **Move safety.** Moving an object on the heap updates its anchor's offset and, because owned fields are inlined, the anchors and ref registrations of any children in the subtree. The full set of fixups is statically known from the type layout — no runtime discovery, no object-side move logic. All refs see the new address on their next dereference.
- **Nulling on destroy.** When an object is destroyed, its anchor iterates all registered refs and nulls them. Any subsequent dereference is a caught error.
- **Leaf-only registration.** A ref registers only in the leaf object's anchor — not in any parent or ancestor. The alternative — registering in every ancestor up the ownership chain — would cost O(depth) on every ref creation and destruction, and force anchor allocation on intermediate objects that may never be ref'd directly. Leaf-only avoids both. When an ancestor is destroyed, recursive teardown reaches every child anyway (to free it and its memory), so checking each child's back-pointer is unavoidable regardless of registration strategy. The check is a single branch per child, almost always not-taken since refs are rare, and effectively free on modern hardware.
- **O(1) ref creation and destruction.** Registering a ref is a push; unregistering is a swap-and-pop.
- **Ref return.** Returning a ref from a function creates a new ref to the same target in the caller's scope. The original ref is destroyed at the end of the returning scope.

For data structure definitions, dereference pseudocode, and the full registration/unregistration/move/destruction protocols, see `memory_layout.md` §Anchors and refs.

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

Destruction order is always deterministic and knowable at compile time. No graph traversal, no cycle detection, no GC pause. For the full destruction protocol pseudocode, see `memory_layout.md` §Destruction protocol.

---

## Error behaviour

Accessing a null ref — one whose owner has been destroyed — is a caught runtime error that terminates the offending branch cleanly. There is no silent undefined behaviour.

---

## Comparison

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
