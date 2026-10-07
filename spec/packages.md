# Zane Packages

This document specifies Zane's package model: how a project's directories form packages, package declarations, imports, member access, visibility, package-scope state, the program and test packages that start a build, and subpackages. Manifests, fetching, and version pinning live in [`dependencies.md`](dependencies.md).

> **See also:** [`lexical.md`](lexical.md) for identifier formation and leading-`_` privacy. [`functions.md`](functions.md) for methods and functions. [`dependencies.md`](dependencies.md) for package identity, manifests, and the dependency graph. [`syntax.md`](syntax.md) §1.5 and §8 for `import`, `package`, and `$` syntax.

---

## 1. Overview

Zane packages are directory-defined namespaces and compilation units that contain every type, function, constant, and other package-scope declaration in the language.

- **`Three kinds of package`.** A project's packages live in three directories beside its manifest: library packages in `lib/`, program packages in `bin/`, and test packages in `test/`.
- **`The directory names the package`.** Every source file declares the name of the directory that holds it, which detects a file copied or moved into the wrong package.
- **`One compilation unit`.** All source files in a package compile together without source-order dependencies.
- **`Library packages share, subpackages nest`.** A project's library packages import one another; a library package may hold subpackages that only it imports.
- **`The import form is the spelling`.** An import makes members of one package available to one source file, written the way the import writes them — qualified, aliased, or bare.
- **`One spelling per entity`.** Whatever an import states is the only way that entity may be written in the file.
- **`No implicit packages`.** A file's own package is established by its `package` declaration and its members remain unqualified. Every other package, `core` included, requires an import.
- **`No hidden ambient state`.** Packages expose immutable constants and verbs; time-varying state lives in values.
- **`Programs and tests start a build`.** A program or test package is the root of its build: it declares `main` and alone reaches the program's console and runtime.

---

## 2. Projects and Packages

### 2.1 A project's packages live in `lib/`, `bin/`, and `test/`

A **project** is the directory that holds a `zane.coda` manifest ([`dependencies.md`](dependencies.md) §2.1). Its packages live in three directories beside the manifest:

- Each directory directly in `lib/` is a **library package**, and a library package may hold subpackages (§2.3).
- Each directory directly in `bin/` is a **program package**: the entry point of one program (§6).
- `test/` holds the project's **test packages** (§7).

```zane
geometry/
  lib/
    math/            package math
    _shaders/        package _shaders
    gui/             package gui
      opengl/        package opengl
  bin/
    viewer/          package viewer
  test/
    math/            package test
  zane.coda
```

A package's source files are the `.zn` files directly in its directory. A `.zn` file directly in `lib/`, `bin/`, or `test/` is a compile-time error, as is a `.zn` file in a subdirectory of a program package. A project holds at least one library or program package.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-projects-packages-move-into-lib-bin-and-test) — "A project's packages move into `lib/`, `bin/` and `test/`".

### 2.2 A package is named by its directory

A library or program package's name is the name of its directory, in camelCase under [`lexical.md`](lexical.md) §3. A library package's name may begin with `_`, which makes the package private to its project (§4.4). Every test package's name is `test` (§7.1).

The names within one project are kept apart:

- No library package is named `test`.
- A program package's name differs from every top-level library package's name, and does not begin with `_`.
- A subpackage's name differs from every top-level library package's name of its project, and does not begin with `_`.

A violation is a compile-time error.

### 2.3 A library package may hold subpackages

A directory inside a library package that holds `.zn` files is a **subpackage**, and a subpackage may hold subpackages of its own, to any depth. The package whose directory holds a subpackage's directory is its **parent**. A subpackage is reached only through its parent (§4.3) and is never visible to another project.

> **Story:** [`stories/packages.md`](../stories/packages.md#subpackages-nest-inside-a-library-package) — "Subpackages nest inside a library package".

### 2.4 Every source file declares its package

Every source file **MUST** begin with a `package packageName` declaration whose name exactly matches its package's name (§2.2). A missing or mismatched declaration is a compile-time error.

```zane
package opengl
```

The directory determines package membership; the declaration asserts that the file is in the package its author intended.

### 2.5 A package is one order-independent compilation unit

All source files of one package form a single compilation unit. Declaration order within a file and file order within the package's directory are semantically irrelevant. A declaration in one file may refer to a declaration in another file of the same package without an import or forward declaration.

> **Story:** [`stories/packages.md`](../stories/packages.md#the-directory-is-the-package) — "The directory is the package".

---

## 3. Imports and Member Access

### 3.1 Imports are file-scoped

An `import` declaration makes members of another package available in the source file containing it. It does not make them available to other files in the current package.

The package must be one the importing package may import (§4.3), named by its package name. A package from another project reaches the importing project through the dependency rules of [`dependencies.md`](dependencies.md) §8.

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

result Float = m$sqrt(value);     // legal
result Float = math$sqrt(value);  // ILLEGAL: the file spells this package `m`
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

### 4.3 Which packages a package may import

What a package may import depends on where it lies in its project:

| Importing package | May import |
|---|---|
| Top-level library package | the project's other top-level library packages, its own subpackages, and the public library packages of the project's dependencies |
| Subpackage | its own subpackages, the project's top-level library packages other than the one it lies within, and the public library packages of the project's dependencies |
| Program package | the project's top-level library packages and the public library packages of its dependencies |
| Test package | as §7.3 states |

A package's **own subpackages** are those whose parent it is; a subpackage of one of them is not among them. A subpackage never imports its parent or a sibling. No package imports a program package or a test package, and the imports among a project's library packages form no cycle ([`dependencies.md`](dependencies.md) §10).

The **public library packages** of a project are its top-level library packages whose names do not begin with `_`. They are the only packages another project reaches.

An import of a package outside these sets is a compile-time error.

> **Story:** [`stories/packages.md`](../stories/packages.md#subpackages-nest-inside-a-library-package) — "Subpackages nest inside a library package".

### 4.4 A leading `_` makes a library package private to its project

A top-level library package whose name begins with `_` is importable by its own project's packages under §4.3 and by no other project. Its declarations follow §4.1 like any package's.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-projects-packages-move-into-lib-bin-and-test) — "A project's packages move into `lib/`, `bin/` and `test/`".

---

## 5. Package-Scope State

### 5.1 Packages contain no mutable variables

Package scope may contain immutable constants and verbs. It **MUST NOT** contain mutable variables or any other time-varying package state.

State that changes over time must live in a value, such as a `struct` or reference-typed object, and reach operations through ordinary parameters, subjects, or capability wiring. This keeps mutation visible to the effect model in [`effects.md`](effects.md).

> **Story:** [`stories/packages.md`](../stories/packages.md#state-has-to-be-a-value) — "State has to be a value".

---

## 6. The Root Package

### 6.1 Program and test packages are roots

Every build that produces a program has one **root package**, from which every other package of the build is reached: the program package it builds, or the test package of a test build (§7.2). A build that compiles only library packages, such as checking or releasing them, has no root package.

Only the root package reaches the `@program$` intrinsic namespace ([`syntax.md`](syntax.md) §2.7), which holds the program's console and runtime ([`effects.md`](effects.md) §6.6). A library package is never a root, so it reaches them only through an instance passed to it.

> **Story:** [`stories/effects.md`](../stories/effects.md#where-the-first-capability-comes-from) — "Where the first capability comes from".

### 6.2 `main` is the entry point

A program starts at `main()`, which the root package declares in any of its source files. `main` takes no parameters, because the root package reaches the program's console and runtime through `@program$` directly. Its return type may be any type, and the value it returns is discarded. It declares no abort type, because no caller exists to handle an abort.

Every program package and every test package **MUST** declare `main`. A package without it is a compile-time error.

```zane
package viewer

import std$

Unit main() {
    console Console(@program$console);
    console!print("hello world");
    return Unit();
}
```

---

## 7. Test Packages

### 7.1 `test/` mirrors `lib/`

Each directory directly in `test/` that holds `.zn` files is a **top-level test package**. A directory deeper in `test/` that holds `.zn` files is a **nested test package**, and its path under `test/` **MUST** match the path of a subpackage under `lib/`: `test/gui/opengl/` tests `lib/gui/opengl/`. A directory of `test/` may hold both `.zn` files and further directories.

```zane
test/
  math/              tests through any top-level library package
  gui/               tests through any top-level library package
    opengl/          tests lib/gui/opengl/
```

Every source file of a test package **MUST** begin with the declaration `package test`.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-librarys-tests-live-in-test-and-import-it-as-a-consumer) — "A library's tests live in `test/` and import it as a consumer".

### 7.2 Each test package is the root of its own test build

A **test build** compiles one test package with the library packages it reaches and runs the result. The test package is its root package (§6.1): it declares `main`, and it alone reaches `@program$`. The library packages are compiled from the project's `lib/` in every test build, and reach the program's console and runtime only through an instance the test package passes to them.

A library package under test is a package of its own, distinct from every release of the same project that the build's dependency graph also reaches. When a dependency of the test package itself depends on the project, the two copies coexist in one program as two versions do ([`dependencies.md`](dependencies.md) §11), and a type of one is not a type of the other.

A build that is not a test build leaves `test/` uncompiled. A project's release archives hold only the objects compiled from `lib/` ([`dependencies.md`](dependencies.md) §3.1).

### 7.3 A test package stands where its package's user stands

A top-level test package may import every top-level library package of its project, including one whose name begins with `_`. A nested test package may import the subpackage it tests and every top-level library package of its project. Both may import the public library packages of the project's `deps` and `test-deps` ([`dependencies.md`](dependencies.md) §2.1), and only a test package may import a package of `test-deps`.

```zane
package test

import core$
import std$
import math

Unit main() {
    console Console(@program$console);
    v math$Vector(3.0, 4.0);
    len Float = v:math$length();
    console!print("length: \%len");
    return Unit();
}
```

A test package reaches what a package's real user reaches. A package's `_`-prefixed declarations are inaccessible from it (§4.1), and a nested test package sees its subpackage as the subpackage's parent does, without the subpackage's own subpackages, which have test packages of their own.

> **Story:** [`stories/packages.md`](../stories/packages.md#subpackages-nest-inside-a-library-package) — "Subpackages nest inside a library package".

---

## 8. Summary

| Concept | Rule |
|---|---|
| Project layout | Library packages in `lib/`, program packages in `bin/`, test packages in `test/`; no `.zn` file directly in any of the three |
| Package identity | A library or program package is named by its directory; every test package is named `test` |
| Package source | The `.zn` files directly in the package's directory |
| Subpackage | A directory with `.zn` files inside a library package; imported only by its parent, never by another project |
| Package declaration | Required in every source file; it matches the package's name |
| Compilation unit | All files in one package compile together; file and declaration order are irrelevant |
| Same-package access | Members are available unqualified across all files in the package |
| Import scope | One source file only |
| Importable packages | A library package imports the project's top-level library packages, its own subpackages, and its dependencies' public library packages, without cycles; nothing imports a program or test package |
| Implicit packages | None; the current package comes from the file's declaration, and every other package requires an explicit import, `core` included |
| Import forms | `import pkg`, `import pkg as alias`, `import pkg$member`, `import pkg$member as alias`, `import pkg$[a, b]`, `import pkg$` |
| Imported member access | Written exactly as the import states it, and no other way |
| Imported name contents | Every accessible declaration of that name: a verb's whole overload set, or a type with its constructors |
| Import reach | Plain-name resolution only; never operator candidates, method lookup, or implicit-constructor applicability |
| Alias casing | An `as` alias keeps the initial case of the name it renames |
| Bare-name collision | Legal only as an overload set of verbs differing in parameter types; otherwise a compile-time error at the import, never shadowing |
| Root package | The program package being built, or the test package of a test build; the only package that reaches `@program$` |
| Entry point | `main()`, declared in any source file of the root package, with no parameters and no abort type; any return type, whose value is discarded; required in every program and test package |
| Test package | Each directory of `test/` with `.zn` files; a nested one mirrors a subpackage and imports it, and each is the root of its own test build |
| Package separator | `$`; distinct from field access and method-call markers |
| Package-private member | Any named package-scope declaration beginning with `_` |
| Project-private package | A top-level library package whose name begins with `_`; never imported by another project |
| Operators | Always public |
| Package state | Immutable constants and verbs only; mutable state lives in values |
