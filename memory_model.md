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
Any owning storage position for a class instance—a symbol, field, or container slot—**MUST NOT** be overwritten after initialization.

```zane
tank Tank(...)
tank = Tank(...) // ILLEGAL
```

This rule eliminates destruction-by-overwrite as a source of dangling refs.

Container overwrite therefore depends on element type. A container element that owns a class instance is single-assignment; a container element whose type is a struct or `ref` remains overwritable.

```zane
owners Array[2]<Node>
refs Array[2]<ref Node>
```

`owners` has owning element slots, so each element is single-assignment once initialized. `refs` has `ref` element slots, so each element may later be rewritten to point at a different live object.

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
- a function or constructor parameter

A `ref` type is legal in storage sites (local symbols, fields, nested storage types) and in function parameter positions. It is not legal in return-type positions.

### 2.5 Refs are repointable
A `ref` symbol or `ref` field may be assigned a different target later, as long as the scope rule in §3.1 is satisfied.

### 2.6 Refs are copied by value
Assigning or passing a `ref` copies the ref value. Rebinding one `ref` storage site later changes only that storage site; it does not retarget other copies.

### 2.7 Refs and owners use the same surface operations
At use sites, a `ref` is used with the same surface syntax as a direct owner. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between owner and `ref` matters only at the storage site: a `ref` stores a non-owning link, while an owner stores the object itself or its owning slot.

### 2.8 Only place expressions are ref-able
A `ref` binding may only be initialized from a **place expression**: a named symbol, a field access, a container element, or a `ref` parameter. Unnamed expression results (temporaries) are not place expressions and cannot be bound to a `ref`.

```zane
engine ref Engine()   // ILLEGAL: temporary is not a place expression
```

```zane
engine Engine()
r ref Engine = engine   // legal: engine is a named, stable storage location
```

Non-`ref` owner bindings may be initialized from any expression, including temporaries.

```zane
engine Engine()   // legal: plain owner binding accepts a temporary
```

### 2.9 `ref` function parameters
A parameter declared as `ref T` requires the caller to supply a place expression. Inside the callee body it acts as a ref binding and may be stored into a `ref` field or `ref` local.

A parameter declared as plain `T` is a **value-only binding**. The caller is not required to supply a place expression, and inside the callee the parameter cannot be stored into a `ref` field.

```zane
class Car {
    engineA ref Engine
    engineB Engine
}

// legal: ref parameter allows storing into ref field
Void consume(this Car, engine ref Engine) mut {
    this.engineA = engine   // legal
    this.engineB = engine   // legal
}

// different overload with a plain parameter
Void inspect(this Car, engine Engine) {
    return this._value + engine.speed   // legal: reading only
}
```

Storing a plain parameter into a `ref` field is illegal:

```zane
Void consumeWrong(this Car, engine Engine) mut {
    this.engineA = engine   // ILLEGAL: plain parameter is not ref-able
}
```

This rule preserves uniform call syntax. The call site writes `consume(e)` or `inspect(e)` regardless of whether the parameter is `ref`. The callee's signature determines whether a place expression is required from the caller.

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

For `ref` fields specifically, the callee must declare the corresponding parameter as `ref T` (§2.9). Attempting to store a plain `T` parameter into a `ref` field is a compile-time error. This means the callee's signature signals whether a place expression is required at the call site.

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
| Single-assignment owning storage | Prevents overwrite-triggered destruction from invalidating refs unexpectedly, including inside owning containers. |
| Structs remain overwritable | Structs are plain values; restricting reassignment would add complexity with no safety benefit. |
| `ref` in parameter positions | Allows callee signatures to express "this argument must be a stable storage location," enabling `ref` field assignment without hidden compiler-invented storage. |
| Place-expression requirement for `ref` | A ref must denote an existing, user-visible object. Binding to a temporary would introduce compiler-invented hidden storage, obscure object identity and lifetime, and contradict the definition of a non-owning reference. |
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
| Owning storage | Class-typed symbols, fields, and container elements are initialized once and never overwritten |
| Struct value | May be overwritten freely |
| `ref` | Non-owning storage; may be repointed and copied by value |
| Place expression | A named symbol, field access, container element, or `ref` parameter; required as the source for any `ref` binding |
| `ref` binding | May only be initialized from a place expression; temporaries are rejected |
| `ref` parameter | Declares that the caller must supply a place expression; the parameter is ref-capable inside the callee |
| Plain `T` parameter | Value-only binding; caller need not supply a place expression; cannot be stored in a `ref` field |
| Ref assignment | Only from a place expression whose owner is in the same or a higher lexical scope than the ref |
| Move | Only into an owner in the same or a higher lexical scope |
| Function call | Callee decides move; caller symbol remains usable |
| Anchor | Lazy, stable indirection for refs across moves |
| Destruction | Deterministic and delayed until the owning scope drains |
