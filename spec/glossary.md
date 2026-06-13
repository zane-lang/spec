# Zane Terminology

This document records the canonical names used across the Zane specification for recurring language concepts and design rules. It does not add semantics; each term points back to the document that defines the rule in full and explains why that label is the preferred name.

> **See also:** [`README.md`](../README.md) for the document index. [`syntax.md`](syntax.md) for canonical surface forms. Topic documents for the full semantic rules behind each term.

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
- **Canonical home:** [`error-handling.md`](error-handling.md) §1

### 2.2 resolve-only shorthand
- **Meaning:** `??` desugars to a `?` handler that only provides a fallback `resolve`.
- **Why this name:** The form is a shorthand for the subset of handler behavior that resolves a replacement value and does nothing else.
- **Canonical home:** [`error-handling.md`](error-handling.md) §3.3

### 2.3 scope-exit guard
- **Meaning:** `guard` conditionally exits the current lexical scope instead of introducing another nested branch.
- **Why this name:** The term emphasizes that `guard` is about leaving the surrounding scope, not about starting a new control-flow block.
- **Canonical home:** [`control-flow.md`](control-flow.md) §3

### 2.4 single-writer rule
- **Meaning:** At most one concurrent `mut` accessor may exist for the same object.
- **Why this name:** The rule is easiest to remember as the requirement that concurrent writing has exactly one writer at a time.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.2

### 2.5 water-tower lifetimes
- **Meaning:** Scope-owned objects stay alive until every `spawn`ed call in that scope has completed and the scope drains.
- **Why this name:** The source document explains the rule through a water-tower analogy in which each still-running spawned call acts like a plate holding the water level up.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.1

### 2.6 structural effect model
- **Meaning:** Effect level is inferred from `mut`, ownership, call structure, and reachable capabilities rather than from a large set of written annotations.
- **Why this name:** The model is "structural" because the compiler derives effects from program structure and reachable state, not from separate effect declarations.
- **Canonical home:** [`effects.md`](effects.md) §1 and §5

### 2.7 capability wiring
- **Meaning:** Capability objects must be passed or stored explicitly so access to external state remains visible in the object graph and call graph.
- **Why this name:** The design treats capabilities like explicit wiring between components rather than ambient globals.
- **Canonical home:** [`effects.md`](effects.md) §6

### 2.8 1-based ordinal counting
- **Meaning:** Counted loops and positional indexing start at `1`, so an ordered sequence's final valid position is its size.
- **Why this name:** The term makes the rule about ordinal positions explicit and distinguishes it from raw numeric arithmetic.
- **Canonical home:** [`control-flow.md`](control-flow.md) §5

---

## 3. Types, Storage, and Binding

### 3.1 place expression
- **Meaning:** A place expression denotes an existing, stable storage location. Some place expressions may create new `&` values, while `[]` expressions remain excluded from that rule.
- **Why this name:** The term names the expressions that refer to a storage "place" rather than to a temporary value.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.2 struct-downstream enforcement
- **Meaning:** A struct may contain only primitives and other legal structs, never a class or `&` field anywhere downstream in nested struct fields.
- **Why this name:** The rule is checked recursively through fields downstream from the outer struct, not just at the first field layer.
- **Canonical home:** [`memory.md`](memory.md) §2.10

### 3.3 unified type parameters
- **Meaning:** A type is parameterized by a single positional `<>` slot system. Each parameter is either a *type parameter* (a `'`-prefixed uppercase name such as `'T`, ranging over types) or a *number parameter* (a lowercase name such as `n`, ranging over compile-time numbers and resolving to a number value in body positions). The two kinds differ only by marker, not by syntactic position.
- **Why this name:** The system unifies what were previously two separate mechanisms — apostrophe generics and bracketed size parameters in different positions — into one slot system distinguished only by the parameter's marker.
- **Canonical home:** [`generics.md`](generics.md) §3

### 3.4 compiler concept types
- **Meaning:** Compiler-provided types such as `@concepts$Number` may appear in parameter positions for literals but not in storage.
- **Why this name:** These are compiler-defined concept-level placeholders for source literals, not ordinary user storage types.
- **Canonical home:** [`syntax.md`](syntax.md) §2.8

### 3.5 field constructor
- **Meaning:** Constructor syntax may declare fields directly in the parameter header and map them into `init{}`.
- **Why this name:** The written constructor header is shaped around fields themselves rather than around separate parameter names.
- **Canonical home:** [`types.md`](types.md) §3.3

### 3.6 method-based privacy
- **Meaning:** `_` fields are private to methods whose first parameter is `this` for that type, rather than to a package boundary.
- **Why this name:** Privacy is granted by the method/receiver relationship, not by where the function is declared.
- **Canonical home:** [`types.md`](types.md) §2.3

### 3.7 direct initialization
- **Meaning:** Every symbol declaration provides its initial value in the declaration itself; bare declarations without an initializer are illegal.
- **Why this name:** The rule is about initialization happening directly at the binding site, not later through control flow.
- **Canonical home:** [`memory.md`](memory.md) §2.11

### 3.8 call-only callable
- **Meaning:** Methods, free functions, and operators may appear only in call position; they have no value form and cannot be referenced as values.
- **Why this name:** The name states the single permitted use site — a call — and contrasts it with the value form that callables deliberately lack.
- **Canonical home:** [`functions.md`](functions.md) §7.1

### 3.9 lambda-variable
- **Meaning:** A symbol bound to a lambda literal. It has one function type and is the only way to hold a function value, since callables themselves are call-only.
- **Why this name:** The term pairs the lambda value with the variable that names it, distinguishing it from an anonymous lambda literal and from a call-only callable.
- **Canonical home:** [`functions.md`](functions.md) §7.3

### 3.10 types as templated functions
- **Meaning:** A type definition takes parameters and is executed to produce a concrete layout, the way a function takes parameters and produces a value. Templating is a direct consequence of types being executable rather than a separate feature.
- **Why this name:** The label states the model directly: a type is a function over its parameters, and applying arguments evaluates it into a concrete type.
- **Canonical home:** [`generics.md`](generics.md) §2

### 3.11 type expression vs constructor call
- **Meaning:** `Type<...>` is a compile-time type expression that applies arguments to a parameterized type and describes architecture; `Type(...)` is a runtime constructor call that builds a value. A constructor call is always by bare name and never carries a `<>` list.
- **Why this name:** The two forms mention the same type name but belong to different systems — the type system versus the value system — so the contrast names the boundary.
- **Canonical home:** [`generics.md`](generics.md) §4 and §5

### 3.12 distinct type vs alias
- **Meaning:** `type Name = T` introduces a new distinct type that is structurally equal to `T` but not interchangeable with it; `alias Name = T` introduces a fully interchangeable name. The keyword carries the distinction.
- **Why this name:** The pairing names the only difference between the two declaration forms — whether the result is a new type or just another name.
- **Canonical home:** [`types.md`](types.md) §5

### 3.13 casing-determined kind
- **Meaning:** The first letter of an identifier selects its lexical class: an uppercase-initial name is a type, a lowercase-initial name is a value, binding, or parameter. A lowercase name in a type position is a compile-time error.
- **Why this name:** Casing alone, not a declaration or lookahead, determines whether a bare name is a type or a value.
- **Canonical home:** [`lexical.md`](lexical.md) §3

---

## 4. Packages, Operators, and Versioning

### 4.1 home-package operator rule
- **Meaning:** An operator implementation may be declared only in the home package of one of its operand types.
- **Why this name:** The rule ties operator declarations to the package that "owns" one operand type and prevents unrelated helper imports from changing operator meaning.
- **Canonical home:** [`operators.md`](operators.md) §2.2

### 4.2 placeholder-prefix rewriting
- **Meaning:** During fetch, a library's `!`-prefixed export symbols are rewritten with the resolved version tag before caching and linking.
- **Why this name:** The committed `!` prefix is only a placeholder marker; the toolchain rewrites that prefix into the real versioned symbol prefix.
- **Canonical home:** [`dependencies.md`](dependencies.md) §6.1

### 4.3 URL identity
- **Meaning:** A package's canonical identity is its full source URL, while local aliases are only import conveniences.
- **Why this name:** The rule says identity comes from the repository URL itself, not from whichever alias a project chooses locally.
- **Canonical home:** [`dependencies.md`](dependencies.md) §1 and §2
