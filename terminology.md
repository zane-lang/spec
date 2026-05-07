# Zane Terminology

This document records the canonical names used across the Zane specification for recurring language concepts and design rules. It does not add semantics; each term points back to the document that defines the rule in full and explains why that label is the preferred name.

> **See also:** [`README.md`](README.md) for the document index. [`syntax.md`](syntax.md) for canonical surface forms. Topic documents for the full semantic rules behind each term.

---

## 1. How to Use This File

This file gives short, reusable names to concepts that appear across multiple spec documents.

- **`Preferred label`.** Each entry records the name the spec should reuse when the same concept appears again.
- **`Preferred casing`.** Terms keep the casing that best matches their role in the spec: formal named models may use capitals, while ordinary reusable noun phrases may stay lowercase.
- **`Meaning`.** Each entry gives only a short summary, not the full rule.
- **`Why this name`.** Each entry explains the connection between the label and the underlying rule.
- **`Canonical home`.** Each entry names the document section where the full rule is specified.

---

## 2. Error Handling, Control Flow, and Concurrency

### 2.1 Bifurcated Return Path
- **Meaning:** An abortable call has a statically typed primary path and a statically typed abort path.
- **Why this name:** The rule splits one call result into two explicit return paths instead of hiding failure in a side channel.
- **Canonical home:** [`error_handling.md`](error_handling.md) §1

### 2.2 resolve-only shorthand
- **Meaning:** `??` desugars to a `?` handler that only provides a fallback `resolve`.
- **Why this name:** The form is a shorthand for the subset of handler behavior that resolves a replacement value and does nothing else.
- **Canonical home:** [`error_handling.md`](error_handling.md) §3.3

### 2.3 scope-exit guard
- **Meaning:** `guard` conditionally exits the current lexical scope instead of introducing another nested branch.
- **Why this name:** The term emphasizes that `guard` is about leaving the surrounding scope, not about starting a new control-flow block.
- **Canonical home:** [`control_flow.md`](control_flow.md) §3

### 2.4 single-writer rule
- **Meaning:** At most one concurrent `mut` accessor may exist for the same object.
- **Why this name:** The rule is easiest to remember as the requirement that concurrent writing has exactly one writer at a time.
- **Canonical home:** [`concurrency_model.md`](concurrency_model.md) §4.2

### 2.5 water-tower lifetimes
- **Meaning:** Scope-owned objects stay alive until every `spawn`ed call in that scope has completed and the scope drains.
- **Why this name:** The source document explains the rule through a water-tower analogy in which each still-running spawned call acts like a plate holding the water level up.
- **Canonical home:** [`concurrency_model.md`](concurrency_model.md) §4.1

### 2.6 structural effect model
- **Meaning:** Effect level is inferred from `mut`, ownership, call structure, and reachable capabilities rather than from a large set of written annotations.
- **Why this name:** The model is "structural" because the compiler derives effects from program structure and reachable state, not from separate effect declarations.
- **Canonical home:** [`purity.md`](purity.md) §1 and §5

### 2.7 capability wiring
- **Meaning:** Capability objects must be passed or stored explicitly so access to external state remains visible in the object graph and call graph.
- **Why this name:** The design treats capabilities like explicit wiring between components rather than ambient globals.
- **Canonical home:** [`purity.md`](purity.md) §6

### 2.8 1-based ordinal counting
- **Meaning:** Counted loops and positional indexing start at `1`, so an ordered sequence's final valid position is its size.
- **Why this name:** The term makes the rule about ordinal positions explicit and distinguishes it from raw numeric arithmetic.
- **Canonical home:** [`control_flow.md`](control_flow.md) §5

---

## 3. Types, Storage, and Binding

### 3.1 place expression
- **Meaning:** A place expression denotes an existing, stable storage location and is therefore legal as a source for `ref` binding.
- **Why this name:** The term names the expressions that refer to a storage "place" rather than to a temporary value.
- **Canonical home:** [`memory_model.md`](memory_model.md) §2.8

### 3.2 struct-downstream enforcement
- **Meaning:** A struct may contain only primitives and other legal structs, never a class or `ref` field anywhere downstream in nested struct fields.
- **Why this name:** The rule is checked recursively through fields downstream from the outer struct, not just at the first field layer.
- **Canonical home:** [`memory_model.md`](memory_model.md) §2.10

### 3.3 binder/reference split
- **Meaning:** `[...]` binds a const parameter in definition positions but refers to an already bound const in type bodies and method `this` types.
- **Why this name:** The same syntax has two roles, so the term highlights the split between binding a name and referring back to one.
- **Canonical home:** [`type_parameters.md`](type_parameters.md) §3 and §4

### 3.4 compiler concept types
- **Meaning:** Compiler-provided types such as `@concepts$Number` may appear in parameter positions for literals but not in storage.
- **Why this name:** These are compiler-defined concept-level placeholders for source literals, not ordinary user storage types.
- **Canonical home:** [`syntax.md`](syntax.md) §2.8

### 3.5 field constructor
- **Meaning:** Constructor syntax may declare fields directly in the parameter header and map them into `init{}`.
- **Why this name:** The written constructor header is shaped around fields themselves rather than around separate parameter names.
- **Canonical home:** [`oop.md`](oop.md) §3.3

### 3.6 method-based privacy
- **Meaning:** `_` fields are private to methods whose first parameter is `this` for that type, rather than to a package boundary.
- **Why this name:** Privacy is granted by the method/receiver relationship, not by where the function is declared.
- **Canonical home:** [`oop.md`](oop.md) §2.3

---

## 4. Packages, Operators, and Versioning

### 4.1 home-package operator rule
- **Meaning:** An operator implementation may be declared only in the home package of one of its operand types.
- **Why this name:** The rule ties operator declarations to the package that "owns" one operand type and prevents unrelated helper imports from changing operator meaning.
- **Canonical home:** [`operators.md`](operators.md) §2.2

### 4.2 placeholder-prefix rewriting
- **Meaning:** During fetch, a library's `!`-prefixed export symbols are rewritten with the resolved version tag before caching and linking.
- **Why this name:** The committed `!` prefix is only a placeholder marker; the toolchain rewrites that prefix into the real versioned symbol prefix.
- **Canonical home:** [`dependency_management.md`](dependency_management.md) §6.1

### 4.3 URL identity
- **Meaning:** A package's canonical identity is its full source URL, while local aliases are only import conveniences.
- **Why this name:** The rule says identity comes from the repository URL itself, not from whichever alias a project chooses locally.
- **Canonical home:** [`dependency_management.md`](dependency_management.md) §1 and §2
