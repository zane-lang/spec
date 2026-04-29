# Zane Memory Model

This document specifies Zane's memory model: ownership, refs, anchors, lexical lifetime rules, and deterministic destruction.

> **See also:** [`oop.md`](oop.md) §2 for classes/structs. [`purity.md`](purity.md) §2 for `mut`. [`concurrency_model.md`](concurrency_model.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 for storage declarations.

---

## 1. Overview

Zane's memory model combines single ownership with lexical lifetime rules to eliminate dangling references without explicit annotations.

- **`Single-assignment owners`.** A symbol or field that owns a class instance is assigned once and never overwritten.
- **`Anchor-tracked refs`.** A `ref` is a non-owning handle tracked through an anchor that updates on moves.
- **`Lexical lifetime rules`.** Ref assignment and ownership moves are allowed only when scopes guarantee safe lifetimes.
- **`Deterministic destruction`.** Objects are destroyed when their owning scope drains, not by garbage collection.

---

## 2. Ownership and Storage

### 2.1 Owners of class instances are single-assignment
A variable or field that owns a **class** instance **MUST NOT** be overwritten after it is initialized. This rule applies to local symbols, parameters, and class fields.

```zane
player Player(...)
player = Player(...) // ILLEGAL: owning class symbol is single-assignment
```

Structs are value types and **MAY** be overwritten freely:

```zane
pos Vec2(1, 2)
pos = Vec2(3, 4) // ok
```

### 2.2 Refs are repointable
A `ref` symbol or `ref` field **MAY** be reassigned to point at a different object at any time, subject to the scope rules in §3.

### 2.3 `ref` is a storage-only modifier
`ref` appears only where values are **stored**:

- local variable declarations
- class fields
- container element types (e.g., `Array[4]<ref Node>`)

`ref` **MUST NOT** appear in function or method signatures. Callers pass a value and the callee decides whether a move occurs (see §3.3).

### 2.4 Ownership is encoded in type positions
Container and field types own their elements by default. The `ref` modifier opts out of ownership:

```zane
Array[4]<Node>       // owns Nodes
Array[4]<ref Node>   // stores non-owning refs

class World {
    player Player
    focus ref Player
}
```

### 2.5 Refs can target named or unnamed values
A `ref` may target a named symbol, a field, a container element, or an unnamed expression result. Anchors are created lazily at the first ref creation, regardless of whether the value is named.

```zane
car Car()
car:register(Node()) // ok: anchor created when ref is stored
```

---

## 3. Scope Rules and Moves

### 3.1 Ref assignment is scope-checked
A `ref` assignment is legal only when the referent's **owner** is declared in the same or a higher lexical scope than the ref. This guarantees the owner outlives the ref.

```zane
r ref Node
{
    node Node()
    r = node // ILLEGAL: node is in a lower scope
}
```

### 3.2 Moves are scope-checked
A value may be moved into a new owning slot only if the destination is declared in the same or a higher lexical scope than the source. Moving into a lower scope is illegal.

```zane
node Node()
{
    owner Node
    owner = node // ILLEGAL: owner is in a lower scope
}
```

### 3.3 Callee-decides moves with silent downgrade
When passing a value to a function, whether a move happens is determined by the callee's body:

- If the callee stores the value into an owning slot, the value moves.
- If it does not, the value remains owned by the caller.

In both cases, the caller's symbol remains valid. If a move occurs, the caller's symbol silently becomes a `ref` via the anchor.

```zane
engine Engine()
car Car(engine)   // engine moves into car.engine
engine:inspect() // still valid: engine is now a ref
```

### 3.4 Mutability is controlled by the method, not by storage
A method marked `mut` is the only authority on mutation. Both owners and refs may call `mut` methods, and both must use the mutating call marker (`!`).

---

## 4. Lifetime and Destruction

### 4.1 Destruction is deterministic
An object is destroyed when its owning storage goes out of scope (or when its owning container/field is destroyed), subject to the water-tower rule in §4.2. Destruction is deterministic and ordered.

### 4.2 Water-tower-drained scopes
If a scope spawns concurrent work, its owned objects are destroyed only after that work finishes. A scope is considered **drained** only when all spawned work within it completes. See [`concurrency_model.md`](concurrency_model.md) §4.1.

### 4.3 Null refs are eliminated
Because refs can only point to owners that outlive them, a ref never observes a destroyed object. The runtime does not expose null refs for user code to handle.

---

## 5. Anchor System

### 5.1 Lazy anchor creation
Anchors are created the first time a ref is made to a value (including silent downgrades after a move). Values with no refs do not allocate anchors.

### 5.2 Anchor updates on moves
When an object moves to a new owning slot, its anchor is updated to the new address. All refs and downgraded owners observe the new location immediately.

### 5.3 Anchor lifetime
Anchors are freed when the owning object is destroyed. Because refs cannot outlive the owner, no runtime ref invalidation is required.

---

## 6. Memory Layout and Allocation

### 6.1 Unified memory region
The runtime reserves a single contiguous region at startup. The heap grows upward; the stack grows downward. If they meet, the program terminates with out-of-memory.

### 6.2 Size-indexed free stacks
Heap allocations (class instances, list storage, anchors) are served from size-indexed free stacks:

- Allocation sizes are rounded up to the nearest 8 bytes.
- Freed slots are pushed onto the stack for that size.
- Allocation pops from the matching stack before bumping the heap frontier.

This provides O(1) allocation and reuse without a general-purpose allocator.

---

## 7. Design Rationale

| Decision | Rationale |
|---|---|
| Single-assignment class owners | Prevents accidental destruction by overwrite and eliminates the most surprising null-ref source. |
| Repointable refs | Separates lifetime responsibility from “current selection” without transfers. |
| Lexical scope rules | Gives a simple, purely lexical lifetime guarantee with no annotations. |
| Callee-decides moves with silent downgrade | Eliminates use-after-move errors while preserving ownership semantics. |
| `ref` only at storage sites | Keeps signatures clean and avoids borrow-annotation burden. |
| Lazy anchor creation | Avoids anchor overhead when refs are never used. |
| Water-tower destruction | Extends lifetime safely into concurrent execution without GC. |

---

## 8. Summary

| Concept | Rule |
|---|---|
| Class owner | Single-assignment; cannot be overwritten after init |
| Struct value | May be overwritten freely |
| `ref` storage | Repointable, non-owning, storage-only modifier |
| Ref assignment | Allowed only from same-or-higher-scope owner |
| Ownership move | Allowed only into same-or-higher-scope owner |
| Function call | Caller symbol stays valid; may silently downgrade to ref |
| Destruction | Happens when the owning scope drains |
