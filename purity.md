# Zane Effect Model

This document specifies Zane's effect model: how `mut` works, how effects are inferred, and how the compiler uses effect data for optimization and concurrency.

> **See also:** [`oop.md`](oop.md) §4 for method declarations. [`concurrency_model.md`](concurrency_model.md) §2 for parallelism rules. [`syntax.md`](syntax.md) §3 for function syntax.

---

## 1. Overview

Zane uses a structural effect model with a single user-facing modifier: `mut`.

- **`No explicit purity tags`.** The compiler infers effect levels; users do not write `pure` or `readonly` annotations.
- **`Method-local mutation`.** `mut` only grants write access to `this` and owned descendants.
- **`Effect signatures`.** The compiler tracks read/write effects on reachable state and capabilities.

---

## 2. Core Definitions

### 2.1 Side effect
A **side effect** is any observable interaction beyond returning a value, including:

- writing to `this` or owned descendants
- reading or writing through a `ref`
- reading or writing through capability objects (I/O, system state)
- allocating or destroying heap objects

### 2.2 Capability
A **capability** is an object whose methods represent access to external state (filesystem, network, console, clock, etc.). Capabilities must be passed explicitly.

### 2.3 `mut`
`mut` is the only effect modifier. It appears on method declarations and grants write access to `this` and objects owned by `this`. A non-`mut` method may not write to `this`.

---

## 3. Effect Inference

### 3.1 Ownership reachability
Effects propagate through ownership: if a method can reach an owned object, it can read it; if it is `mut`, it may write to it.

### 3.2 Ref access
Reading through a `ref` is a **read** effect on the referenced object. Writing through a `ref` is illegal; mutation must go through `mut` on `this`.

### 3.3 Call graph propagation
A function’s effect signature is the union of the effects of the functions it calls, plus any local effects it performs.

---

## 4. Effect Levels

### 4.1 Total Pure
No side effects and guaranteed termination for all inputs. The compiler may evaluate Total Pure functions at compile time when inputs are known.

### 4.2 Pure
No side effects, but termination is not proven. Pure functions are parallelizable but not compile-time evaluated.

### 4.3 Read-Only Impure
Reads external state (via refs or capabilities) but performs no writes. Read-only effects are parallelizable with other reads.

### 4.4 Write Impure
Writes to state (via `mut` on `this` or via writable capabilities). Write effects are serialized against conflicting reads/writes.

---

## 5. Compiler Guarantees

### 5.1 Compile-time evaluation
Total Pure calls with statically known inputs are evaluated at compile time and removed from the runtime graph.

### 5.2 Concurrency safety
Effect signatures are used to enforce the single-writer rule and to serialize conflicting capability accesses. See [`concurrency_model.md`](concurrency_model.md) §4.

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Single `mut` modifier | Keeps the surface language small while still expressing mutation intent. |
| Inferred effect levels | Avoids annotation burden and keeps signatures stable. |
| Separate Total Pure vs Pure | Allows safe compile-time evaluation without assuming termination. |
| Capability-based effects | Makes external state access explicit and analyzable. |
