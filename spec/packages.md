# Zane Packages

This document specifies Zane's package model: package declarations and member naming. Manifests, fetching, and version pinning live in [`dependencies.md`](dependencies.md).

> **See also:** [`types.md`](types.md) for classes and structs. [`functions.md`](functions.md) for methods and free functions. [`dependencies.md`](dependencies.md) for package identity, manifests, and the import system. [`syntax.md`](syntax.md) §8 for `package` and `$` syntax.

---

## 1. Overview

Zane packages are namespaces that host every type, function, and constant in the language.

- **`PascalCase namespaces`.** Package names use PascalCase, matching the casing convention for type names.
- **`Explicit member access`.** Cross-package members are addressed with `PackageName$member`.
- **`No hidden ambient state`.** Packages expose constants and functions; time-varying state must live in objects and be passed explicitly.

---

## 2. Package Declarations and Member Access

### 2.1 Packages are PascalCase namespaces
`package PackageName` introduces a namespace. Package names use PascalCase, like type names. Members are referenced as `PackageName$member`.

### 2.2 Imports
A package becomes available with `import PackageName`. After import, both qualified (`PackageName$member`) and unqualified references (subject to method-lookup rules in [`functions.md`](functions.md) §6) are usable.

> **See also:** [`dependencies.md`](dependencies.md) §8 for the import syntax and resolution rules across the dependency graph.

### 2.3 Package scope contains no hidden mutable ambient state
Packages may expose immutable constants and package-scope functions. Time-varying state must live in objects and be passed or stored explicitly.

---

## 3. Design Rationale

| Decision | Rationale |
|---|---|
| PascalCase package names | Aligns package names with type-naming conventions, making qualified member access (`PackageName$member`) visually consistent with the rest of the language. |
| `$` as the member separator | A non-letter separator removes any ambiguity between package-qualified access and ordinary identifier characters such as `.` (used for field access). |
| No hidden mutable package state | Prevents ambient state from undermining ownership and effect reasoning. |
