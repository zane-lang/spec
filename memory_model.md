# Zane Memory Model

This document specifies Zane's memory model: ownership, refs, anchors, lexical lifetime rules, heap layout, and deterministic destruction.

> **See also:** [`oop.md`](oop.md) §2 for classes and structs. [`purity.md`](purity.md) §2 for `mut`. [`concurrency_model.md`](concurrency_model.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling references by combining single ownership, lexical lifetime rules, and anchor-based tracking of non-owning references.

- **`Single-assignment owners`.** A class owner is initialized once and never overwritten.
- **`Repointable refs`.** A `ref` is non-owning storage that can point at different objects over time.
- **`Lexical lifetime enforcement`.** Ref assignment and ownership moves are checked using declaration scope alone.
- **`Deterministic destruction`.** Objects are destroyed when their owning scope drains; there is no tracing garbage collector.

---

## 2. Ownership and Storage

### 2.1 Every class instance has exactly one owner
Every class instance is owned by exactly one symbol, field, or container slot at a time. Ownership is the default storage mode for class values.

### 2.2 Class owners are single-assignment
An owning symbol or field for a class instance **MUST NOT** be overwritten after initialization.

```zane
tank Tank(...)
tank = Tank(...) // ILLEGAL
```

This rule eliminates destruction-by-overwrite as a source of dangling refs.

### 2.3 Struct values are freely overwritable
Structs are value types with no anchor and no heap identity. Reassigning a struct overwrites the storage slot directly.

```zane
pos Vec2(1, 2)
pos = Vec2(3, 4) // ok
```

### 2.4 `ref` is non-owning storage
`ref` creates non-owning storage. A `ref` may be declared as:

- a local symbol
- a class field
- an element type inside another storage type

`ref` does not appear in function signatures.

### 2.5 Refs are repointable
A `ref` symbol or `ref` field may be assigned a different target later, as long as the scope rule in §3.1 is satisfied.

### 2.6 Named and unnamed values are equally ref-able
Refs may target named symbols, fields, container elements, or unnamed expression results. Anchor creation is triggered by the first ref, not by whether the value has a source-level name.

---

## 3. Scope Rules and Moves

### 3.1 Ref assignment uses owner scope
A `ref` assignment is legal only when the target's owner is declared in the same or a higher lexical scope than the ref itself.

```zane
r ref Node
{
    node Node()
    r = node // ILLEGAL
}
```

The compiler compares declaration scopes. It does not perform borrow inference or lifetime annotation solving.

### 3.2 Ownership moves also use lexical scope
A value may move into a new owner only when the destination owner is declared in the same or a higher lexical scope than the source owner.

```zane
node Node()
{
    owner Node
    owner = node // ILLEGAL
}
```

### 3.3 Callee-decides move semantics
When a value is passed to a function, the callee decides whether a move happens by what it does internally:

- storing the value in an owning slot moves it
- not storing it leaves ownership in place

### 3.4 Caller symbols stay valid after moves
If a callee moves a value away from the caller, the caller's symbol silently downgrades to a `ref` through the anchor. Zane therefore has no user-visible use-after-move error class.

```zane
engine Engine()
car Car(engine)
engine:inspect() // valid: engine is now a ref
```

---

## 4. Lifetime and Destruction

### 4.1 Destruction is deterministic
Class instances are destroyed when their owner dies, their owning container dies, or their owning scope drains under the concurrency rules.

### 4.2 Scopes drain before destruction
If a scope launches concurrent work, objects owned by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule.

### 4.3 Ref storage never extends lifetime
Refs do not participate in ownership and cannot prolong object lifetime. They only track a live object whose owner is already guaranteed to outlive them.

### 4.4 Null refs are not a user-facing state
Because scope rules prevent refs from outliving their owners, the runtime does not expose a normal “null ref” programming model to the user.

---

## 5. Memory Layout

### 5.1 Single reserved memory region
The runtime reserves one contiguous region at startup. The heap grows upward and the stack grows downward. If they meet, the program terminates with out-of-memory.

### 5.2 Heap allocation uses size-indexed free stacks
Heap allocations reuse previously freed slots by exact rounded size:

- requested sizes are rounded to 8-byte boundaries
- each rounded size has its own free stack
- allocation first pops from the matching free stack, then bumps the frontier if needed

This keeps allocation and free O(1) and avoids coalescing logic.

### 5.3 Struct and class layout follow declaration order
Fields are laid out in declaration order. Structs are stored inline. Class objects carry heap identity and anchor metadata as required by the implementation.

### 5.4 Booleans may be packed
The compiler may pack booleans in structs and stack frames when doing so does not change language semantics.

---

## 6. Anchors and Refs

### 6.1 Anchors are created lazily
Objects that are never ref'd pay no anchor-allocation cost. The first ref creation allocates and initializes the anchor.

### 6.2 Anchors are stable indirection points
A ref follows an anchor rather than pointing directly at an object. If an object moves, the anchor is updated and all refs observe the new location.

### 6.3 Moves update anchors, not all refs
Object relocation is O(1) with respect to the number of refs, because only the anchor location record changes.

### 6.4 Destroying an object frees its anchor
When the owning object is destroyed, its anchor is torn down as part of destruction. Since refs cannot outlive the owner, this does not create a dangling-user-reference state.

---

## 7. Language Comparisons

### 7.1 Ownership and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner by default | ✅ | ❌ | ❌ | ✅ |
| Non-owning refs as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Ref counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Refs remain usable across moves | ✅ via anchors | ❌ | ❌ | ⚠️ only when borrow checking permits the move pattern |
| Overwrite destroys ownership source | prohibited by language rules | possible | possible | ⚠️ prevented by borrow checker |

### 7.2 Memory behavior

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Destruction timing | deterministic | non-deterministic | deterministic | manual / RAII |
| GC pauses | ❌ | ✅ | ❌ | ❌ |
| Allocation reuse | exact-size free stacks | runtime-managed | allocator-dependent | allocator-dependent |
| Dangling ref risk | ❌ | ❌ | ❌ | ✅ |
| Lifetime annotations | ❌ | ❌ | ✅ | ❌ |

---

## 8. Design Rationale

| Decision | Rationale |
|---|---|
| Single-assignment class owners | Prevents overwrite-triggered destruction from invalidating refs unexpectedly. |
| Structs remain overwritable | Structs are plain values; restricting reassignment would add complexity with no safety benefit. |
| `ref` is storage-only | Keeps lifetime responsibility visible at storage sites without polluting signatures. |
| Lexical scope rules | A simple same-or-higher-scope rule is easy to implement and explain. |
| Callee-decides moves with caller downgrade | Preserves ownership while avoiding Rust-style use-after-move friction. |
| Lazy anchors | Objects with no refs do not pay ref-tracking costs. |
| Anchor indirection | Makes object moves O(1) with respect to the number of refs. |
| Shared size-indexed free stacks | Keeps allocation predictable and avoids general-purpose allocator overhead. |
| Water-tower destruction | Extends the same ownership model into concurrent execution without adding GC or async lifetimes. |

---

## 9. Summary

| Concept | Rule |
|---|---|
| Class owner | Initialized once and never overwritten |
| Struct value | May be overwritten freely |
| `ref` | Non-owning storage; may be repointed |
| Ref assignment | Only from owners in the same or a higher lexical scope |
| Move | Only into an owner in the same or a higher lexical scope |
| Function call | Callee decides move; caller symbol remains usable |
| Anchor | Lazy, stable indirection for refs across moves |
| Destruction | Deterministic and delayed until the owning scope drains |
