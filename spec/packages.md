# Zane Packages

This document specifies Zane's package model: directory-defined namespaces and compilation units, package declarations, imports, member access, visibility, and package-scope state. Manifests, fetching, and version pinning live in [`dependencies.md`](dependencies.md).

> **See also:** [`lexical.md`](lexical.md) for identifier formation and leading-`_` privacy. [`functions.md`](functions.md) for methods and functions. [`dependencies.md`](dependencies.md) for package identity, manifests, and the dependency graph. [`syntax.md`](syntax.md) §1.5 and §8 for `import`, `package`, and `$` syntax.

---

## 1. Overview

Zane packages are directory-defined namespaces and compilation units that contain every type, function, constant, and other package-scope declaration in the language.

- **`Directory identity`.** A package's name is the basename of its directory.
- **`Declaration check`.** Every source file declares that package name, allowing the compiler to detect a file copied or moved into the wrong directory.
- **`One compilation unit`.** All source files in a package compile together without source-order dependencies.
- **`Explicit cross-package access`.** An import makes one package namespace available to one source file; its members remain qualified as `packageName$member`.
- **`No hidden ambient state`.** Packages expose immutable constants and verbs; time-varying state lives in values.

---

## 2. Package Identity and Compilation

### 2.1 The directory basename is the package name

Every source directory defines one package. The package name is the basename of that directory and uses camelCase under [`lexical.md`](lexical.md) §3.

For example, every source file directly inside a directory named `httpClient` belongs to the package `httpClient`.

### 2.2 Every source file declares its package

Every source file **MUST** begin with a `package packageName` declaration whose name exactly matches the basename of the file's directory. A missing or mismatched declaration is a compile-time error.

```zane
package httpClient
```

The directory determines package membership; the declaration asserts that the file is in the directory its author intended.

### 2.3 A package is one order-independent compilation unit

All source files directly in one package directory form a single compilation unit. Declaration order within a file and file order within the directory are semantically irrelevant. A declaration in one file may refer to a declaration in another file of the same package without an import or forward declaration.

> **Story:** [`stories/packages.md`](../stories/packages.md#the-directory-is-the-package) — "The directory is the package".

---

## 3. Imports and Member Access

### 3.1 Imports are file-scoped namespace availability

`import packageName` makes that package namespace available in the source file containing the import. It does not make the namespace available to other files in the current package and does not inject any of the imported package's members as unqualified names.

The package must be available through the dependency rules in [`dependencies.md`](dependencies.md).

### 3.2 Current-package members are unqualified

A source file may refer to any accessible declaration in its own package by its unqualified name, including declarations from other files in the same package.

### 3.3 Imported members remain qualified

A member of an imported package **MUST** be referenced as `packageName$member`. Importing a package never makes `member` alone resolve to that package.

```zane
import math

result Float = math$sqrt(value)
```

The method-call lookup rules in [`functions.md`](functions.md) §6 are a distinct resolution mechanism. A qualified extension-method call writes the package name explicitly as `receiver:packageName$method(...)`.

### 3.4 `$` separates a package namespace from its member

The parser treats the left operand of `$` as a package namespace and the right operand as one of its members. `$` is distinct from `.`, which is field access, and from `:` and `!`, which mark method calls.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-barrier-that-still-joins-the-name) — "A barrier that still joins the name".

---

## 4. Package Visibility

### 4.1 Leading `_` makes a named declaration package-private

A named package-scope declaration whose name begins with `_` is accessible from every source file in its own package and inaccessible from every other package. This applies to all named declarations, including types, aliases, constants, functions, methods, and constructors. The leading underscore does not change the identifier's lexical class; see [`lexical.md`](lexical.md) §4.2.

An access from another package is illegal even when it uses an explicit `packageName$` qualifier.

> **Story:** [`stories/lexical.md`](../stories/lexical.md#privacy-lives-in-the-name) — "Privacy lives in the name".

### 4.2 Operators are public

Operators are symbol-named rather than identifier-named and cannot carry a leading `_`. Every operator declaration is therefore public.

---

## 5. Package-Scope State

### 5.1 Packages contain no mutable variables

Package scope may contain immutable constants and verbs. It **MUST NOT** contain mutable variables or any other time-varying package state.

State that changes over time must live in a value, such as a `struct` or reference-typed object, and reach operations through ordinary parameters, receivers, or capability wiring. This keeps mutation visible to the effect model in [`effects.md`](effects.md).

> **Story:** [`stories/packages.md`](../stories/packages.md#state-has-to-be-a-value) — "State has to be a value".

---

## 6. Summary

| Concept | Rule |
|---|---|
| Package identity | The basename of a source directory |
| Package declaration | Required in every source file and must match the directory basename |
| Compilation unit | All files in one package compile together; file and declaration order are irrelevant |
| Same-package access | Members are available unqualified across all files in the package |
| Import scope | One source file only |
| Imported member access | Always qualified as `packageName$member` |
| Package separator | `$`; distinct from field access and method-call markers |
| Package-private member | Any named package-scope declaration beginning with `_` |
| Operators | Always public |
| Package state | Immutable constants and verbs only; mutable state lives in values |
