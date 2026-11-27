# Zane Packages

This document specifies Zane's package model: how a project's directories form packages, package declarations, imports, member access, visibility, package-scope state, the program and test packages that start a build, and subpackages. Manifests, fetching, and version pinning live in [`dependencies.md`](dependencies.md).

> **See also:** [`lexical.md`](lexical.md) for identifier formation and leading-`_` privacy. [`functions.md`](functions.md) for methods and functions. [`dependencies.md`](dependencies.md) for package identity, manifests, and the dependency graph. [`syntax.md`](syntax.md) §1.5 and §8 for `import`, `package`, and `$` syntax.

---

## 1. Overview

Zane packages are directory-defined namespaces and compilation units that contain every type, function, constant, and other package-scope declaration in the language.

- **`Three kinds of package`.** A project's packages live in three directories beside its manifest: library packages in `lib/`, program packages in `bin/`, and test packages in `test/`.
- **`The directory names the package`.** Every source file of a library or program package declares the name of the directory that holds it, so that a file copied or moved into the wrong package is detected; every test package's files declare `package test` (§7.1).
- **`One compilation unit`.** All source files in a package compile together without source-order dependencies.
- **`Library packages share, subpackages nest`.** A project's library packages import one another; a library package may hold subpackages that only it imports.
- **`The import form is the spelling`.** An import makes members of one package available to one source file, written in the form the import gives them — qualified, aliased, or bare.
- **`One spelling per entity`.** The spelling an import gives an entity is the only way the file may write that entity.
- **`No implicit packages`.** A file's own package is established by its `package` declaration and its members remain unqualified. Every other package, `core` included, requires an import.
- **`No hidden ambient state`.** Packages expose immutable constants and verbs; time-varying state lives in values.
- **`Programs, plugins and tests start a build`.** A program, plugin, or test package is the root of its build. A program or test package declares `main`; a plugin package declares none and builds a dynamic library that a host loads (§8).
- **`Exports are entry points`.** A package-scope lambda marked `export "cName"` is a C function a host may call. It has the rights `main` has, and only a root package's code calls it (§8.2, §8.4).

---

## 2. Projects and Packages

### 2.1 A project's packages live in `lib/`, `bin/`, and `test/`

A **project** is the directory that holds a `zane.coda` manifest ([`dependencies.md`](dependencies.md) §2.1). Its packages live in three directories beside the manifest:

- Each directory directly in `lib/` is a **library package**, and a library package may hold subpackages (§2.3).
- Each directory directly in `bin/` is a **program package**, the entry point of one program (§6), or a **plugin package**, which builds a dynamic library for a host to load (§8). A `bin/` package that declares `main` is a program package; one that declares no `main` is a plugin package.
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

A package's source files are the `.zn` files directly in its directory. A package may also hold C code, which [`ffi.md`](ffi.md) §2.1 specifies. A `.zn` file directly in `lib/`, `bin/`, or `test/` is a compile-time error, as is a `.zn` file in a subdirectory of a program package. A project holds at least one library or program package.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-projects-packages-move-into-lib-bin-and-test) — "A project's packages move into `lib/`, `bin/` and `test/`".

### 2.2 A package is named by its directory

A library or program package's name is the name of its directory, in camelCase under [`lexical.md`](lexical.md) §3. A library package's name may begin with `_`, which makes the package private to its project (§4.4). Every test package's name is `test` (§7.1).

A project may reach a dependency's library package under another name, which its manifest gives it ([`dependencies.md`](dependencies.md) §2.1). The package keeps its own name in its own project.

The names within one project are kept apart:

- No library package is named `test`.
- A program package's name differs from every top-level library package's name, and does not begin with `_`.
- A subpackage's name differs from every top-level library package's name of its project, and does not begin with `_`.

A violation is a compile-time error. A subpackage's name also differs from every name under which its project reaches a dependency's package, which dependency resolution checks ([`dependencies.md`](dependencies.md) §8).

### 2.3 A library package may hold subpackages

A directory inside a library package that holds `.zn` files is a **subpackage**, and a subpackage may hold subpackages of its own, to any depth. The package whose directory holds a subpackage's directory is its **parent**. A subpackage is reached only through its parent (§4.3) and is never visible to another project.

> **Story:** [`stories/packages.md`](../stories/packages.md#subpackages-nest-inside-a-library-package) — "Subpackages nest inside a library package".

### 2.4 Every source file declares its package

Every source file **MUST** begin with a `package packageName` declaration whose name exactly matches its package's name (§2.2). A missing or mismatched declaration is a compile-time error.

```zane
package opengl;
```

The directory determines package membership; the declaration asserts that the file is in the package its author intended.

### 2.5 A package is one order-independent compilation unit

All source files of one package form a single compilation unit. Declaration order within a file and file order within the package's directory are semantically irrelevant. A declaration in one file may refer to a declaration in another file of the same package without an import or forward declaration.

> **Story:** [`stories/packages.md`](../stories/packages.md#the-directory-is-the-package) — "The directory is the package".

---

## 3. Imports and Member Access

### 3.1 Imports are file-scoped

An `import` declaration makes members of another package available in the source file containing it. It does not make them available to other files in the current package.

The package must be one the importing package may import (§4.3), named by its package name, or for a dependency's package by the name its project's manifest gives it. A package from another project reaches the importing project through the dependency rules of [`dependencies.md`](dependencies.md) §8.

No other package is available without an import. A file's own package is established by its `package` declaration, and its members remain available unqualified (§3.2). There is no ambient or automatically-imported package. `core` is not an exception: a file that writes `Int` imports it like any other dependency (see [`types.md`](types.md) §2.6), most often with the whole-package form `import core$` (§3.3).

### 3.2 Current-package members are unqualified

A source file may refer to any accessible declaration in its own package by its unqualified name, including declarations from other files in the same package.

### 3.3 The import form fixes how its members are written

The import form sets how the file writes the imported members at each use site.

| Form | Members are written as |
|---|---|
| `import pkg` | `pkg$member` |
| `import pkg as alias` | `alias$member` |
| `import pkg$member` | `member` |
| `import pkg$member as alias` | `alias` |
| `import pkg$[memberA, memberB]` | `memberA`, `memberB` |
| `import pkg$` | every accessible member, unqualified |

```zane
import math;
import math as m;
import math$sqrt;
import math$sqrt as root;
import math$[sqrt, pow];
import math$;
```

The bracket form is an ordinary flat list under [`lexical.md`](lexical.md) §6.2: `,` separates entries and never trails. The `pkg$` form ends the qualifier at the separator and takes everything past it; it brings only members the importing package may access, so a `_`-prefixed declaration (§4.1) is never included.

For a dependency's package, `pkg` is the name the project's manifest reaches it under: its own name, or the name a `renames` row gives it ([`dependencies.md`](dependencies.md) §8).

An import is a **spelling**, not a linkage. Every imported name resolves to its fully qualified declaration before any later stage, and the package name a compiled symbol carries is always the defining package's own name (see [`dependencies.md`](dependencies.md) §6.1), whatever spelling the importing file chose.

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.4 Each imported entity has exactly one spelling

An import gives each imported entity one spelling, and the file may use only that spelling. `import math as m` makes `m$sqrt` legal and `math$sqrt` illegal; `import math$sqrt as root` makes `root` legal and `sqrt` illegal.

```zane
import math as m;

result Float = m$sqrt(value);     // legal
result Float = math$sqrt(value);  // ILLEGAL: the file spells this package `m`
```

Two imports that would give one entity two spellings in the same file are a compile-time error, so `import math` and `import math$sqrt` cannot both appear.

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.5 An imported name carries every declaration that shares it

A `pkg$member` import brings every accessible package-scope declaration in `pkg` named `member`. For a verb that means the whole overload set: an overloaded name is a set of candidates that only a call site collapses ([`functions.md`](functions.md) §7.1), so importing one member of the set is not expressible. For a type name it means the type together with its constructors ([`types.md`](types.md) §3.1) and its named constructors (§3.4), so an imported type can be constructed. A method is a declaration an import brings only when its subject is a type parameter (§3.6).

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.6 Imports affect plain-name resolution only

An import changes how plain names resolve and nothing else. It does not contribute operator candidates ([`operators.md`](operators.md) §2.2), does not affect method lookup except as a plain name does ([`functions.md`](functions.md) §6.1), and does not affect which implicit constructors apply at a coercion site, which the home-package rule of [`types.md`](types.md) §4.5 settles.

Methods and operators are therefore not importable members, because neither is reached by a plain name:

```zane
import shapes$area;  // ILLEGAL: a method is reached by subject:pkg$method()
import math$+;       // ILLEGAL: operators resolve by operand home package
```

A cross-package method call is written with the qualifier at the call site instead ([`functions.md`](functions.md) §6.2).

A method whose subject is a type parameter ([`generics.md`](generics.md) §3.7) is the exception, because no subject type's home reaches it: its plain name reaches it, so an import brings it as it brings a function, and the spelling the import gives is the one method lookup searches ([`functions.md`](functions.md) §6.1, step 3). A name a package declares both for a subject of any type and for a subject type of its own is importable, and the import brings only the methods for a subject of any type; the others stay reached through their subject type's home, as before.

```zane
import core$work;    // legal: `work` is declared for a subject of any type
machine!work(step);
```

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.7 An alias preserves the casing class

An `as` alias **MUST** have the same initial case as the name it renames, because an initial capital is semantic ([`lexical.md`](lexical.md) §3). A value name cannot be aliased to a type-shaped name, or the reverse.

```zane
import math$sqrt as root;     // legal: both lowercase
import math$Vector as Vec;    // legal: both uppercase
import math$sqrt as Root;     // ILLEGAL: a value renamed to a type-shaped name
```

> **Story:** [`stories/packages.md`](../stories/packages.md#what-an-import-writes-is-what-the-file-writes) — "What an import writes is what the file writes".

### 3.8 Colliding bare names are an error at the import

Two imports may bring one bare name into a file only when the result is a legal overload set: every declaration sharing the name must be a verb, and they must differ in the ordered parameter types that decide overload identity ([`functions.md`](functions.md) §4.1). Dispatch is then an ordinary call-site choice.

Any other collision is a compile-time error reported at the import declaration rather than at the use site, so the conflict shows in the file's own import list. A lambda-variable is a symbol rather than a verb and can never accumulate an overload set ([`functions.md`](functions.md) §7.3), so a collision involving one is always an error. A bare name that collides with a member of the file's own package (§3.2) is an error on the same terms; an import never shadows.

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

A `_` field draws the same boundary: it is private to the package that declares its type ([`types.md`](types.md) §2.3).

> **Story:** [`stories/lexical.md`](../stories/lexical.md#privacy-lives-in-the-name) — "Privacy lives in the name".

### 4.2 Operators are public

Operators are named by operator tokens rather than identifiers and cannot carry a leading `_`. Every operator declaration is therefore public.

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

A project reaches each public library package of its dependencies under the name its manifest gives it, which is the package's own name unless a `renames` row renames or drops it ([`dependencies.md`](dependencies.md) §2.1 and §8). A package the manifest drops is in none of the sets of the table above, for any of the project's packages.

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

### 6.1 Program, plugin, and test packages are roots

Every build that produces a program or a dynamic library has one **root package**, from which every other package of the build is reached: the program package it builds, the plugin package of a dynamic-library build (§8.1), or the test package of a test build (§7.2). A build that compiles only library packages, such as checking or releasing them, has no root package.

Only the root package reaches the `@program$` intrinsic namespace ([`syntax.md`](syntax.md) §2.7), which holds the program's console and runtime ([`effects.md`](effects.md) §6.6), and only it declares foreign variables, which give the program's foreign capabilities ([`ffi.md`](ffi.md) §5.1). A library package is never a root, so it reaches them only through an instance passed to it. An exported lambda is the one exception: wherever it is declared, it has the rights of root-package code (§8.4).

> **Story:** [`stories/effects.md`](../stories/effects.md#where-the-first-capability-comes-from) — "Where the first capability comes from".

### 6.2 `main` is the entry point

A program starts at `main()`, which the root package declares in any of its source files. `main` takes no parameters, because the root package reaches the program's console and runtime through `@program$` directly. Its return type may be any type, and the value it returns is discarded.

The program itself calls `main`, so it is the caller that handles an abort of `main` (§6.3). The one abort type `main` may declare is `@primitives$String`, the message the program reports. Any other abort type on `main` is a compile-time error.

Every program package and every test package **MUST** declare `main`. A test package without it is a compile-time error, and a `bin/` package without it is a plugin package (§8.1). A plugin package has no `main`, because its host calls its exports instead.

```zane
package viewer;

import core$;

Unit main() {
    console Console(@program$console);
    console!print("hello world");
    return Unit();
}
```

> **Story:** [`stories/packages.md`](../stories/packages.md#main-may-abort-with-a-primitivesstring-and-the-program-ends-with-a-status) — "`main` may abort with a `@primitives$String`, and the program ends with a status".

### 6.3 A program ends with an exit status

A program ends with an **exit status**, a number it hands to whatever started it. How the program ends determines the status:

- **`main` returns.** The status is `0`. A program whose printed output could not all be written ends with status `1` instead.
- **`main` aborts.** The program writes what it printed before the abort, then writes the abort's string, followed by a line feed, to its standard error stream. The status is `1`, whether or not those writes succeed.
- **The program stops.** A stop, such as an index out of range or a call that nests too deep ([`memory.md`](memory.md) §3.7), is not an abort, and no handler sees it. The status is not `0`.

A dynamic library runs inside its host's process, so for a dynamic library the program is that process (§8.1). A stop inside a dynamic library ends the host's whole process, as a stop ends a program. An abort never reaches the host, because an exported lambda declares no abort type (§8.2).

```zane
Unit?@primitives$String main() {
    console Console(@program$console);
    settings String = readSettings() ? reason {
        abort @primitives$String("cannot read the settings: " + reason);
    }
    console!print(settings);
    return Unit();
}
```

An abort is not a coercion site ([`types.md`](types.md) §4.2), so a `core` `String` message is converted to `@primitives$String` explicitly, through the implicit constructor `core` declares for it ([`types.md`](types.md) §4.1).

> **Story:** [`stories/packages.md`](../stories/packages.md#main-may-abort-with-a-primitivesstring-and-the-program-ends-with-a-status) — "`main` may abort with a `@primitives$String`, and the program ends with a status".

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

A **test build** compiles one test package with the library packages it reaches and runs the result. The test build passes when the program ends with status `0`, and fails otherwise (§6.3). The test package is its root package (§6.1): it declares `main`, and apart from exported lambdas (§8.4) it alone reaches `@program$`. A test package may carry `export` lines (§8.3); the test build then gives the exports they name C symbols, so the test package's own C code can play a host and call them. The library packages are compiled from the project's `lib/` in every test build, and reach the program's console and runtime only through an instance the test package passes to them.

A library package under test is a package of its own, distinct from every release of the same project that the build's dependency graph also reaches. When a dependency of the test package itself depends on the project, the two copies coexist in one program as two versions do ([`dependencies.md`](dependencies.md) §11), and a type of one is not a type of the other.

A build that is not a test build leaves `test/` uncompiled. A project's release archives hold only the objects compiled from `lib/` ([`dependencies.md`](dependencies.md) §3.1).

### 7.3 A test package stands where its package's user stands

A top-level test package may import every top-level library package of its project, including one whose name begins with `_`. A nested test package may import the subpackage it tests and every top-level library package of its project. Both may import the public library packages of the project's `deps` and `test-deps` ([`dependencies.md`](dependencies.md) §2.1), and only a test package may import a package of `test-deps`.

```zane
package test;

import core$;
import math;

Unit main() {
    console Console(@program$console);
    v math$Vector(3.0, 4.0);
    len Float = v:math$length();
    console!print("length: \%len");
    return Unit();
}
```

A test package has the same access as a package that imports the package under test. A package's `_`-prefixed declarations are inaccessible from it (§4.1), and a nested test package sees its subpackage as the subpackage's parent does, without the subpackage's own subpackages, which have test packages of their own.

> **Story:** [`stories/packages.md`](../stories/packages.md#subpackages-nest-inside-a-library-package) — "Subpackages nest inside a library package".

## 8. Plugin Packages and Exports

### 8.1 A plugin package builds a dynamic library

A **plugin package** is a package in `bin/` that declares no `main` (§2.1). The toolchain builds it as a **dynamic library** — a `.so`, `.dylib`, or `.dll` — for a **host** to load into its own process and call, instead of as an executable. A plugin package holds `export` lines (§8.3), and may also declare exported lambdas (§8.2) and hold C code of its own ([`ffi.md`](ffi.md) §2.1). A `bin/` package with neither `main` nor any `export` line or exported lambda is a compile-time error, and so is one that declares `main` and an `export` line.

```zane
package synthclap;

import synth;

export clap<synth$Synth, synth$Dsp, synth$SynthInfo>;
```

The dynamic library's C symbols are the plugin package's own exported lambdas, the exports of every library its `export` lines name, and the symbols of the C code in the packages it reaches ([`ffi.md`](ffi.md) §2.5). A symbol a host looks up by a fixed name, such as CLAP's `clap_entry`, comes from C code, which calls the exported lambdas.

When a host loads the dynamic library, the toolchain's start-up code creates `@program$console` and `@program$runtime` ([`effects.md`](effects.md) §6.6), once for each loaded file, so two Zane dynamic libraries in one host have two runtimes. `@program$runtime:arguments()` gives an empty list, because a host passes no command line.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-dynamic-library-is-a-package-without-main) — "A dynamic library is a package without `main`".

### 8.2 An exported lambda is a C function

A package-scope lambda-variable ([`syntax.md`](syntax.md) §3.8) followed by `export "cName"` is an **exported lambda**, in any package. In a build that exports it (§8.3) it becomes a C function named `cName`, which calls the lambda:

```zane
create ^Synth(rate @primitives$F64) {
    return Synth(rate);
} export "synth_create"

setVolume Unit(this Synth, volume @primitives$F64) mut {
    this.volume = Float(volume);
    return Unit();
} export "synth_set_volume"
```

Only a lambda is exported, never a verb, because a lambda is the one callable that is a value ([`functions.md`](functions.md) §7.1): an exported lambda hands a Zane function value to C as a foreign variable hands a C global to Zane ([`ffi.md`](ffi.md) §5.1). Visibility does not decide exports, and a leading `_` does not keep a lambda from being exported. An exported lambda introduces no type or number parameter, since C passes no type, and declares no abort type, since no Zane caller handles its abort.

Each parameter and the result of an exported lambda is one of the types a function value passed to C may take ([`ffi.md`](ffi.md) §3.6), or a **C-held object** (§8.5):

- a result `^T`, `T` a reference type, hands C an owner of a new C-held object;
- a parameter `T`, or a subject `this T`, borrows a C-held object, and a subject `this T mut` borrows it mutably;
- a parameter `^T` takes a C-held object's owner back from C, which gives up its pointer.

A result may also be a value whose lists cross as `zane_span` views; an exported lambda returns one only when its subject is `this T mut`, and the views stay valid until the next call that borrows that subject mutably or takes its owner (§8.5).

Every C name is unique in a dynamic library or test build. Two exports of one name in one build are a compile-time error.

> **Story:** [`stories/packages.md`](../stories/packages.md#a-dynamic-library-is-a-package-without-main) — "A dynamic library is a package without `main`".

### 8.3 `export` lines choose a build's exports

An `export` line names a library package whose exported lambdas the build gives C symbols: `export somelib;`, or, for a parameterized library, an instance of it, `export clap<synth$Synth, synth$Dsp, synth$SynthInfo>;`. Only a plugin package or a test package (§7.2) carries `export` lines.

An exported lambda becomes a C symbol only in two kinds of build:

- a dynamic-library build, when the lambda lies in the plugin package or in a library one of its `export` lines names;
- a test build, when the lambda lies in the test package or in a library one of the test package's `export` lines names.

In any other build an exported lambda stays an ordinary lambda constant. The `export` lines live in the root package and not in the library, because which exports go into one file is a decision about one build: one library may be exported into a CLAP plugin by one plugin package and into a Python module by another.

### 8.4 Exports have entry-point rights

An exported lambda is an entry point, as `main` is: the operating system calls `main`, and a host calls exported lambdas. In every build, an exported lambda therefore has the rights of root-package code (§6.1): its body may reach `@program$` and declare foreign variables ([`ffi.md`](ffi.md) §5.1), and it may write the program's console, runtime, and foreign capabilities ([`effects.md`](effects.md) §3). Two rules keep the capability model intact:

- Only a root package's code may call an exported lambda. A root already holds every capability, so calling an entry point gives it nothing it lacked.
- A library package's code calls no exported lambda, its own included, so a library gains no outside access that no caller handed it.

An object that keeps a capability past the call that made it stores it at construction, as any object does ([`effects.md`](effects.md) §6.2).

### 8.5 C holds an object in a scope of its own

An owner that an exported lambda returns to C settles in a **top-level scope** of its own, nested in no other scope, because a host creates and destroys objects in any order. C holds the object as an opaque pointer, never reads or writes through it, and passes it back only as an argument of exported lambdas. An exported lambda whose parameter takes the owner as `^T` takes it from C, and the owner is then an ordinary `^T` parameter ([`lifetimes.md`](lifetimes.md) §1.1): it dies when that lambda's scope drains unless the body moves it elsewhere, and once it has left, the top-level scope drains and releases its memory ([`memory.md`](memory.md) §3.1). A host that never gives an object back leaks it.

```zane
create ^SynthInstance() {
    return SynthInstance(synth$Synth(), synth$Params());
} export "synth_create"

destroy Unit(instance ^SynthInstance) {
    return Unit();
} export "synth_destroy"
```

Neither of two top-level scopes outlives the other, so the store rule ([`lifetimes.md`](lifetimes.md) §1.1) rejects an `&` from one C-held object into another. A C-held object may still reference package constants, capabilities, and its own parts. The only path to a C-held object is therefore the argument C passes, which is what lets a host call exported lambdas from several threads at once under the rules of [`concurrency.md`](concurrency.md) §4.7.

A value an exported lambda returns with its subject `this T mut` lives in that subject's top-level scope, so the `zane_span` views of its lists stay valid after the lambda returns, until the next call that borrows the subject mutably or takes its owner.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#c-holds-a-zane-object-in-a-scope-of-its-own) — "C holds a Zane object in a scope of its own".

---

## 9. Summary

| Concept | Rule |
|---|---|
| Project layout | Library packages in `lib/`, program and plugin packages in `bin/`, test packages in `test/`; no `.zn` file directly in any of the three |
| Package identity | A library, program, or plugin package is named by its directory; every test package is named `test` |
| Package source | The `.zn` files directly in the package's directory, beside any C code ([`ffi.md`](ffi.md) §2.1) |
| Subpackage | A directory with `.zn` files inside a library package; imported only by its parent, never by another project; its name differs from every name its project reaches a dependency's package under |
| Package declaration | Required in every source file; it matches the package's name |
| Compilation unit | All files in one package compile together; file and declaration order are irrelevant |
| Same-package access | Members are available unqualified across all files in the package |
| Import scope | One source file only |
| Importable packages | A library package imports the project's top-level library packages, its own subpackages, and its dependencies' public library packages under the names its manifest gives them, without cycles; nothing imports a program or test package |
| Implicit packages | None; the current package comes from the file's declaration, and every other package requires an explicit import, `core` included |
| Import forms | `import pkg`, `import pkg as alias`, `import pkg$member`, `import pkg$member as alias`, `import pkg$[a, b]`, `import pkg$` |
| Imported member access | Written in the spelling the import gives it, and no other way |
| Imported name contents | Every accessible declaration of that name: a verb's whole overload set, or a type with its constructors; of the methods, only those for a subject of any type (§3.6) |
| Import reach | Plain-name resolution only; never operator candidates, method lookup beyond a method declared for a subject of any type, which its plain name reaches, or implicit-constructor applicability |
| Alias casing | An `as` alias keeps the initial case of the name it renames |
| Bare-name collision | Legal only as an overload set of verbs differing in parameter types; otherwise a compile-time error at the import, never shadowing |
| Root package | The program package being built, the plugin package of a dynamic-library build, or the test package of a test build; the only package that reaches `@program$`, apart from exported lambdas |
| Entry point | `main()`, declared in any source file of the root package, with no parameters, and either no abort type or `@primitives$String`; any return type, whose value is discarded; required in every program and test package; a `bin/` package without it is a plugin package |
| Exit status | `0` when `main` returns and its output was written; `1` when `main` aborts, after an attempt to write its message and a line feed to standard error, whether or not the write succeeds; not `0` when the program stops |
| Test result | Passed when the program ends with status `0` |
| Plugin package | A `bin/` package with no `main` and with `export` lines or exported lambdas; built as a dynamic library a host loads; its console and runtime are created once per loaded file |
| Exported lambda | A package-scope lambda followed by `export "cName"`; a C function in a build that exports it; no type parameters, no abort type; boundary types of a C function value, plus C-held objects; C names unique per build |
| `export` line | `export lib;` or `export lib<Args>;` in a plugin or test package; gives that library's exported lambdas C symbols in this build |
| Entry-point rights | An exported lambda has root-package rights in every build; only a root package's code calls one |
| C-held object | An owner returned to C, in a top-level scope of its own; borrowed by `T` or `this T mut`, taken back by `^T`; no `&` between two of them |
| Stop in a dynamic library | Ends the host's process |
| Test package | Each directory of `test/` with `.zn` files; a nested one mirrors a subpackage and imports it, and each is the root of its own test build |
| Package separator | `$`; distinct from field access and method-call markers |
| Package-private member | Any named package-scope declaration beginning with `_` |
| Project-private package | A top-level library package whose name begins with `_`; never imported by another project |
| Operators | Always public |
| Package state | Immutable constants and verbs only; mutable state lives in values |
