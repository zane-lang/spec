# Zane Packages

This document specifies Zane's package model: directory-defined namespaces and compilation units, package declarations, imports, member access, visibility, and package-scope state. Manifests, fetching, and version pinning live in [`dependencies.md`](dependencies.md).

> **See also:** [`lexical.md`](lexical.md) for identifier formation and leading-`_` privacy. [`functions.md`](functions.md) for methods and functions. [`dependencies.md`](dependencies.md) for package identity, manifests, and the dependency graph. [`syntax.md`](syntax.md) §1.5 and §8 for `import`, `package`, and `$` syntax.

---

## 1. Overview

Zane packages are directory-defined namespaces and compilation units that contain every type, function, constant, and other package-scope declaration in the language.

- **`Directory identity`.** A package's name is the basename of its directory.
- **`Declaration check`.** Every source file declares that package name, allowing the compiler to detect a file copied or moved into the wrong directory.
- **`One compilation unit`.** All source files in a package compile together without source-order dependencies.
- **`The import form is the spelling`.** An import makes members of one package available to one source file, written the way the import writes them — qualified, aliased, or bare.
- **`One spelling per entity`.** Whatever an import states is the only way that entity may be written in the file.
- **`No implicit packages`.** A file's own package is established by its `package` declaration and its members remain unqualified. Every other package, `core` included, requires an import.
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

### 3.1 Imports are file-scoped

An `import` declaration makes members of another package available in the source file containing it. It does not make them available to other files in the current package.

The package must be available through the dependency rules in [`dependencies.md`](dependencies.md).

No other package is available without an import. A file's own package is established by its `package` declaration, and its members remain available unqualified (§3.2). There is no ambient or automatically-imported package. `core` is not an exception: a file that writes `Int` imports it like any other dependency (see [`types.md`](types.md) §2.6), most often with the whole-package form `import core$` (§3.3).

### 3.2 Current-package members are unqualified

A source file may refer to any accessible declaration in its own package by its unqualified name, including declarations from other files in the same package.

### 3.3 The import form fixes how its members are written

What an import writes after `import` is what the file writes at the use site.

| Form | Members are written as |
|---|---|
| `import pkg` | `pkg$member` |
| `import pkg as alias` | `alias$member` |
| `import pkg$member` | `member` |
| `import pkg$member as alias` | `alias` |
| `import pkg$[memberA, memberB]` | `memberA`, `memberB` |
| `import pkg$` | every accessible member, unqualified |

```zane
import math
import math as m
import math$sqrt
import math$sqrt as root
import math$[sqrt, pow]
import math$
```

The bracket form is an ordinary flat list under [`lexical.md`](lexical.md) §6.2: `,` separates entries and never trails. The `pkg$` form ends the qualifier at the separator and takes everything past it; it brings only members the importing package may access, so a `_`-prefixed declaration (§4.1) is never included.

An import is a **spelling**, not a linkage. Every imported name resolves to its fully qualified declaration before any later stage, and the package name a compiled symbol carries is always the defining package's own name (see [`dependencies.md`](dependencies.md) §6.1), whatever spelling the importing file chose.

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.4 Each imported entity has exactly one spelling

An import states one spelling and that spelling is the only one available in the file. `import math as m` makes `m$sqrt` legal and `math$sqrt` illegal; `import math$sqrt as root` makes `root` legal and `sqrt` illegal.

```zane
import math as m

result Float = m$sqrt(value)      // legal
result Float = math$sqrt(value)   // ILLEGAL: the file spells this package `m`
```

Two imports that would give one entity two spellings in the same file are a compile-time error, so `import math` and `import math$sqrt` cannot both appear.

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.5 An imported name carries every declaration that shares it

A `pkg$member` import brings every accessible package-scope declaration in `pkg` named `member`. For a verb that means the whole overload set: an overloaded name is a set of candidates that only a call site collapses ([`functions.md`](functions.md) §7.1), so importing one member of the set is not expressible. For a type name it means the type together with its constructors ([`types.md`](types.md) §3.1) and its named constructors (§3.4), so an imported type can be constructed.

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.6 Imports affect plain-name resolution only

An import changes how plain names resolve and nothing else. It does not contribute operator candidates ([`operators.md`](operators.md) §2.2), does not affect method lookup ([`functions.md`](functions.md) §6.1), and does not affect which implicit constructors apply at a coercion site, which the home-package rule of [`types.md`](types.md) §4.5 settles.

Methods and operators are therefore not importable members, because neither is reached by a plain name:

```zane
import shapes$area   // ILLEGAL: a method is reached by subject:pkg$method()
import math$+        // ILLEGAL: operators resolve by operand home package
```

A cross-package method call is written with the qualifier at the call site instead ([`functions.md`](functions.md) §6.2).

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.7 An alias preserves the casing class

An `as` alias **MUST** have the same initial case as the name it renames, because an initial capital is semantic ([`lexical.md`](lexical.md) §3). A value name cannot be aliased to a type-shaped name, or the reverse.

```zane
import math$sqrt as root      // legal: both lowercase
import math$Vector as Vec     // legal: both uppercase
import math$sqrt as Root      // ILLEGAL: a value renamed to a type-shaped name
```

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.8 Colliding bare names are an error at the import

Two imports may bring one bare name into a file only when the result is a legal overload set: every declaration sharing the name must be a verb, and they must differ in the ordered parameter types that decide overload identity ([`functions.md`](functions.md) §4.1). Dispatch is then an ordinary call-site choice.

Any other collision is a compile-time error reported at the import declaration rather than at the use site, so the file's own import list states the conflict. A lambda-variable is a symbol rather than a verb and can never accumulate an overload set ([`functions.md`](functions.md) §7.3), so a collision involving one is always an error. A bare name that collides with a member of the file's own package (§3.2) is an error on the same terms; an import never shadows.

The narrower forms of §3.3 are the remedy: where `import pkg$` would collide, `import pkg$member` or an `as` alias takes only what the file needs.

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.9 `$` separates a package namespace from its member

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

State that changes over time must live in a value, such as a `struct` or reference-typed object, and reach operations through ordinary parameters, subjects, or capability wiring. This keeps mutation visible to the effect model in [`effects.md`](effects.md).

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
| Implicit packages | None; the current package comes from the file's declaration, and every other package requires an explicit import, `core` included |
| Import forms | `import pkg`, `import pkg as alias`, `import pkg$member`, `import pkg$member as alias`, `import pkg$[a, b]`, `import pkg$` |
| Imported member access | Written exactly as the import states it, and no other way |
| Imported name contents | Every accessible declaration of that name: a verb's whole overload set, or a type with its constructors |
| Import reach | Plain-name resolution only; never operator candidates, method lookup, or implicit-constructor applicability |
| Alias casing | An `as` alias keeps the initial case of the name it renames |
| Bare-name collision | Legal only as an overload set of verbs differing in parameter types; otherwise a compile-time error at the import, never shadowing |
| Package separator | `$`; distinct from field access and method-call markers |
| Package-private member | Any named package-scope declaration beginning with `_` |
| Operators | Always public |
| Package state | Immutable constants and verbs only; mutable state lives in values |
