# Zane Effect Model

This document specifies Zane's effect model: `mut`, inferred effect levels, capability access, structural inference, and the compiler guarantees built on top of those rules.

> **See also:** [`oop.md`](oop.md) §4 for method declarations. [`concurrency_model.md`](concurrency_model.md) §2 and §4 for parallelism and conflict rules. [`error_handling.md`](error_handling.md) §5 for the connection between effects and abortability.

---

## 1. Overview

Zane uses a structural effect model with a single user-facing effect modifier: `mut`.

- **`No purity keywords`.** Users do not write `pure`, `readonly`, or capability qualifiers.
- **`Receiver-local mutation`.** `mut` only grants write access to `this` and its owned subtree.
- **`Compiler-inferred effect levels`.** The compiler classifies code by what state it can read or write.
- **`Capability-based external effects`.** I/O and external state remain explicit because capability objects must be passed or stored.

---

## 2. Core Definitions

### 2.1 Side effect
A side effect is any observable interaction beyond returning a value, including:

- writing to `this` or owned descendants
- reading through a `ref`
- interacting with capability objects
- allocating or destroying heap objects

### 2.2 Capability
A capability is an object whose methods model access to external state, such as a filesystem, logger, socket, clock, or random source.

### 2.3 `mut`
`mut` is the only effect modifier in the language. It appears on methods and grants write access to `this` and values owned by `this`.

### 2.4 Parameters are not mutable by default
Parameters other than `this` are read-only. Mutation of another object must be expressed by calling a `mut` method on that object as the receiver.

---

## 3. Inferred Effect Levels

The compiler assigns a function to the strongest effect level required by any operation in its body or any function it calls transitively. Reads through refs/capabilities raise a function out of the pure levels; writes to receiver-owned or external state raise it to Write Impure.

### 3.1 Level 1 — Total Pure
Total Pure functions depend only on explicit parameters and immutable package constants. They have no side effects and are guaranteed to terminate for all inputs.

### 3.2 Level 2 — Pure
Pure functions have no side effects but are not proven total. They are still reorderable and parallelizable at runtime, but they are not compile-time evaluated automatically.

### 3.3 Level 3 — Read-Only Impure
Read-Only Impure functions read mutable state through refs or read-only capabilities but do not write.

### 3.4 Level 4 — Write Impure
Write Impure functions mutate `this`, mutate capability-backed state, or otherwise perform externally observable writes.

---

## 4. Effect Enforcement

### 4.1 Non-`mut` methods cannot write `this`
A method without `mut` may not assign to fields of `this` or call `mut` methods on `this` or owned descendants.

### 4.2 `mut` does not authorize arbitrary writes
Even a `mut` method may write only within the receiver-owned subtree. It does not gain permission to mutate unrelated parameters.

### 4.3 Ref access is read-only by default
Reading through a `ref` is an effectful read. Mutation of referenced state must still be expressed through a `mut` method call on a receiver that has the proper ownership or capability relationship.

---

## 5. Structural Inference

### 5.1 Ownership graph drives receiver effects
The compiler uses the ownership graph of `this` to determine which fields and descendants are writable in a `mut` method and readable in any method.

### 5.2 Call-graph propagation
If a function calls another function, its effect classification must be at least as strong as the called function's relevant effects.

### 5.3 Refs raise effect level
A function that reads through a `ref` is not Total Pure, even if it performs no writes, because the observed value may vary over time.

### 5.4 Unknown callees are conservatively classified
If the compiler cannot prove the effect behavior of a callee, it must classify the call site conservatively rather than assuming purity. In practice this means unknown callees are treated as requiring the strongest effect level needed to preserve safety.

---

## 6. Capability Wiring and Explicit State Flow

### 6.1 Capabilities must be passed or stored explicitly
There is no ambient global I/O capability. Code can affect external state only through capability objects it receives directly or via ownership.

### 6.2 Constructor injection is ordinary capability wiring
Capabilities may be stored into objects at construction time. This does not create ambient authority; it only records an explicit ownership path by which later methods can reach the capability.

### 6.3 `ref` fields can also expose read access paths
Storing a `ref` field is another explicit way to make state reachable. Because refs observe mutable state outside the current ownership subtree, they raise the containing method or function out of the pure levels when read.

### 6.4 Context objects are explicit, not magical
A "context object" that groups several capabilities is just another ordinary object in the ownership graph. It may reduce parameter count, but it does not hide effects from the compiler because the reachable capabilities are still explicit in storage and call structure.

### 6.5 Prop drilling is intentional
Passing capabilities through constructors and methods is part of the design. It keeps effects visible in object structure rather than hidden in ambient module state.

---

## 7. Constructors, Allocation, and Abortability

### 7.1 Constructors may allocate but are not `mut`
Constructors create values and therefore participate in allocation, but they do not mutate an existing receiver.

### 7.2 Allocation and destruction are effectful implementation events
Heap allocation and destruction are observable to the compiler's optimizer and scheduler even when the source code does not expose them as explicit method calls.

### 7.3 Abortability is orthogonal
A function's abort type and effect level are independent. An abortable function may be Total Pure, Read-Only Impure, or Write Impure depending on what else it does.

---

## 8. Concurrency Implications

### 8.1 Total Pure and Pure work are natural parallelization candidates
Because they do not write mutable state, they can be reordered and parallelized subject to profitability heuristics.

### 8.2 Read-only effects compose with the single-writer rule
Multiple concurrent reads are legal. A read that conflicts with a concurrent write must be serialized by the compiler/runtime.

### 8.3 Receiver-local mutation composes with ownership
Two `mut` calls on different receiver instances may run in parallel. Two `mut` calls on the same instance must be serialized.

---

## 9. Effect Level Matrix

| Level | Reads refs/capabilities | Writes receiver-owned state | May write external state | Compile-time evaluation |
|---|---|---|---|---|
| Total Pure | ❌ | ❌ | ❌ | ✅ |
| Pure | ❌ | ❌ | ❌ | ❌ |
| Read-Only Impure | ✅ | ❌ | ❌ | ❌ |
| Write Impure | ⚠️ may | ✅ possible | ⚠️ may | ❌ |

---

## 10. Design Rationale

| Decision | Rationale |
|---|---|
| Single user-facing effect modifier | Keeps the language small while still making mutation explicit. |
| Structural inference instead of effect annotations | Lets the compiler derive power from ownership and call structure without burdening signatures. |
| Distinguish Total Pure from Pure | Enables safe compile-time evaluation without assuming termination. |
| Capabilities are explicit objects | Prevents hidden ambient effects and keeps dependency flow visible. |
| `mut` is receiver-scoped | Aligns mutation permissions with ownership rather than arbitrary parameter aliasing. |
| Refs count as effectful reads | A ref observes state whose value can change outside the current function. |
