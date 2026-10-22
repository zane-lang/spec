# Zane C Interface

This document specifies how a Zane package calls C: the C code a package holds and how the toolchain builds it, foreign verbs and the types that cross into C, foreign handles, and what foreign code is taken to read and write.

> **See also:** [`effects.md`](effects.md) §5 and §6 for capability access and compile-time evaluation. [`memory.md`](memory.md) §2.9 for passing modes. [`dependencies.md`](dependencies.md) §3 and §6 for release archives and symbol rewriting. [`syntax.md`](syntax.md) §2.16 and §3.11 for the declaration forms.

> **Story:** [`stories/ffi.md`](../stories/ffi.md) tells the story behind this design.

---

## 1. Overview

A package may hold C code beside its Zane source. The toolchain compiles that C for the build's target, links it into the package, and lets the package's own declarations call it. A call into C is an ordinary call in Zane: it has a Zane signature, Zane passing modes, and the effects that signature states.

- **`The toolchain builds the C`.** A package's C sources are compiled by the toolchain's own C compiler, for whatever target the build is for, with settings from a `c.coda` file in the package's directory. No build script runs.
- **`Foreign verbs`.** A function or method whose body is a C function, named by its C symbol. Its Zane signature is checked against the C declaration in the package's interface headers.
- **`C never writes Zane memory`.** Zane values cross into C as values or as read-only views for the duration of the call. State that C keeps lives in C memory, owned from Zane through a **foreign handle** that releases it when it is destroyed.
- **`Outside state is a capability`.** A foreign verb that reaches no capability is taken to read and write nothing outside the program, so its calls fold at compile time like any other computation. A foreign verb that reads or writes outside state says so by being a method on the system capability, `@runtime$System`, or on a foreign capability.
- **`The declaration is trusted`.** The compiler cannot see what C code touches. A foreign verb declared without a capability that in fact reads outside state is evaluated at compile time, and the value the build machine produced is built into the program (§5.5).

> **Story:** [`stories/ffi.md`](../stories/ffi.md#a-c-interface-that-keeps-compile-time-evaluation) — "A C interface that keeps compile-time evaluation".

---

## 2. A Package's C Code

### 2.1 C sources and interface headers

Every package may hold C code: a library, program, or test package, and a subpackage. Its C code is:

- its **C sources**: the `.c` files directly in the package's directory, together with any its `c.coda` lists (§2.3);
- its **interface headers**: the `.h` files directly in the package's directory.

A foreign verb of the package names a C function that one of its interface headers declares (§3.7). Headers elsewhere, including those an interface header includes, are seen by the C compiler and by nothing else.

```zane
zlib/
  lib/
    zlib/
      zlib.zn          package zlib: the foreign verbs
      binding.h        interface header
      binding.c        C source
      c.coda           C build settings
      vendor/          the upstream library's own tree
        deflate.c
        zlib.h
        ...
  zane.coda
```

A directory inside a package that holds no `.zn` file is not a subpackage ([`packages.md`](packages.md) §2.3), so a vendored tree such as `vendor/` is part of the package that holds it.

A package's C code is reached only through that package's foreign verbs. Another package, in the same project or another, calls those verbs like any other.

### 2.2 The toolchain compiles a package's C code

The toolchain compiles each C source of a package as C17, with the C compiler it carries, for the target the build is for. One machine therefore builds a package's C for every target, as it builds its Zane.

Two settings are fixed for every C source, and no setting of a package's `c.coda` changes them:

- floating-point expressions are evaluated as written, each operation rounded on its own, with no contraction of a multiply and an add into one operation;
- the C compiler makes no assumption that a floating-point value is finite or that its operations are associative.

So a C floating-point computation gives the same result each time it is evaluated, at compile time or at run time (§5.4).

The toolchain runs no program the package supplies to build its C: no build script, configure step, or other build system. What a package's C build needs is stated as data in its `c.coda` (§2.3).

The C code of a package is compiled once per build, with the settings of that package alone. Two packages' C code never share a compilation, so a definition in one package's settings has no effect on another's.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#the-toolchain-builds-the-c-and-cmake-does-not) — "The toolchain builds the C, and CMake does not".

### 2.3 A package's `c.coda`

A package whose C build needs settings holds a file named `c.coda` directly in its directory. It is written by hand, as the package's source is, and no command records anything in it. Its one table, `c`, gives one setting per row:

```zane
c [
    setting  value            target
    source   vendor/*.c       *
    include  vendor           *
    define   HAVE_UNISTD_H=1  *-linux-*
    define   HAVE_UNISTD_H=1  *-apple-*
]
```

Each row records:

- **setting**: one of the settings below.
- **value**: the setting's value.
- **target**: a pattern over the target triple, in which `*` matches any run of characters. The row applies to a build whose target the pattern matches; `*` alone applies to every build.

The settings belong to the package whose directory holds the file, so they move with it: a package moved to another directory or another project keeps its C build, and the project's manifest and lock file ([`dependencies.md`](dependencies.md) §2) record nothing about C. A package without a `c.coda` compiles the C sources in its directory with no settings of its own.

The settings are:

| Setting | Value | Effect |
|---|---|---|
| `source` | a path or a pattern, relative to the package's directory | the matching `.c` files are C sources of the package |
| `include` | a directory, relative to the package's directory | the C compiler searches it for included headers |
| `define` | `NAME` or `NAME=VALUE` | the C compiler defines the macro for every C source of the package |
| `archive` | a path, relative to the package's directory | a prebuilt static library linked with the package (§2.4) |
| `system` | a library name | a library of the target system linked with the program (§2.4) |

A path **MUST** stay inside the project; one that is absolute or leaves the project through `..` is an error. A `source` pattern that matches no file in a build it applies to is an error. The rows apply in the order they are written.

### 2.4 Prebuilt archives and system libraries

C code reaches a build in one of three forms:

- **from source**: the package's C sources, compiled by the toolchain (§2.2);
- **as a prebuilt archive**: an `archive` row names a static library the project holds, built for the targets its `target` pattern matches;
- **from the system**: a `system` row names a library of the target system, such as `m` or `user32`, which the program links against.

Building for a target needs what each applying row names for that target. The toolchain carries, for every target it builds for, the system libraries a Zane program links already. Any other system library, and the headers that declare it, is supplied to the build for its target. A build that lacks one fails with an error naming the package and the library.

The three forms differ in what the compiler can evaluate ahead of time. C code compiled from source is evaluated at compile time where a call to it folds (§5.4). C code from an archive or from the system is run only by the running program.

### 2.5 A package's C symbols belong to the package

Every C function and variable that a package's C sources or archives define with external linkage is renamed into the package's own namespace when the package is compiled. In a library package it carries the `!` placeholder and is rewritten with the package's version and identity hash, like the package's Zane symbols ([`dependencies.md`](dependencies.md) §6.1). References to them from the package's own C code and Zane code are renamed with them.

So two versions of one package, or two packages that hold the same C library, coexist in one program as their Zane code does ([`dependencies.md`](dependencies.md) §11). A symbol the package's C code references and does not define, such as a function of the C library or of a `system` row's library, keeps its name, and every package that references it reaches the same definition.

### 2.6 Release archives hold the C objects

A project's release archive holds the objects compiled from its library packages' C sources beside their Zane objects ([`dependencies.md`](dependencies.md) §3.1), for the archive's target. Its prebuilt archives are part of its source tree. A consumer compiles against a dependency's source ([`dependencies.md`](dependencies.md) §14), C sources and interface headers included, so the compiler reads a dependency's C code as it reads its Zane code: to check foreign verbs against their declarations (§3.7) and to evaluate calls at compile time (§5.4).

A `release` dependency's objects were compiled by that package's own compiler ([`dependencies.md`](dependencies.md) §14), which may compile its C differently from the consumer's. So the archive records, for each C source and for the code the toolchain generates around the package's foreign verbs, what the object was compiled from: the preprocessed source, the compile command, and the compiler's version. The consumer evaluates a call into the dependency's C at compile time only where what it compiles from the dependency's source matches that record. Where it does not, the call runs only in the running program, as a call into an archive does (§2.4).

> **Story:** [`stories/ffi.md`](../stories/ffi.md#the-toolchain-builds-the-c-and-cmake-does-not) — "The toolchain builds the C, and CMake does not".

---

## 3. Foreign Verbs

### 3.1 A foreign verb's body is a C function

A **foreign verb** is a function or method whose body is a C function. In place of a body, it names the C function's symbol:

```zane
@primitives$I64 crc32(crc @primitives$I64, data @primitives$String) foreign "zane_crc32"
```

A call to a foreign verb calls the C function with the arguments the call passes, converted as §3.3 states, and gives its result converted back. Everything else about the verb is Zane: it is declared at package scope, it overloads, it is imported, it may be private through a leading `_`, and its calls follow its passing modes and its kind ([`effects.md`](effects.md) §3).

The symbol is the C function's name as the interface header declares it. A foreign verb may be a function, a method, or a `mut` method. It is never a constructor, an operator, or a subscript, and it takes no type or number parameter, no concept-typed parameter, and no block.

A package's own Zane code wraps its foreign verbs in the verbs it offers. The foreign verbs are usually private to the package:

```zane
package zlib

import core$

@primitives$I64 _crc32(crc @primitives$I64, data @primitives$String) foreign "zane_crc32"

Int crc32(data String) => Int(_crc32(@primitives$I64(0), data))
```

> **See also:** [`syntax.md`](syntax.md) §3.11 for the declaration forms.

### 3.2 Parameters become the C function's arguments in order

A foreign verb's parameters are the C function's parameters, in order. A method's subject is the first of them, except a subject of type `@runtime$System`, which C does not receive (§5.1).

### 3.3 Types at the boundary

A foreign verb's parameter and result types are drawn from the table below. Each corresponds to C types, and the C declaration's type **MUST** be one of them (§3.7).

| Zane type | C type | Crosses as |
|---|---|---|
| `@primitives$I32` | `int32_t`, `uint32_t`, `int`, `unsigned int` | its 32 bits |
| `@primitives$I32` | `int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `char`, `signed char`, `unsigned char`, `short`, `unsigned short` | its low bits when passed; widened by the C type's sign when returned |
| `@primitives$I64` | `int64_t`, `uint64_t`, `long long`, `unsigned long long`, and, on a target whose pointers are 64 bits, `size_t`, `ptrdiff_t`, `intptr_t`, `uintptr_t` | its 64 bits |
| `@primitives$F32` | `float` | its value |
| `@primitives$F64` | `double` | its value |
| `@primitives$Bool` | `bool` | its value |
| `@primitives$Unit` | `void`, as a result only | nothing |
| `@primitives$String` | `zane_bytes` | a view of its bytes (§3.4); a returned view is copied (§3.5) |
| `@primitives$Array<T, n>`, `T` a scalar primitive | `const E *`, `E` a full-width C type of `T` | a view of its `n` elements (§3.4) |
| `@primitives$List<T>`, `T` a scalar primitive | `zane_span`, its elements full-width C types of `T` | a view of its elements (§3.4) |
| a C-compatible value type (below) | a `struct` of the same layout | its value |
| a foreign handle `H` (§4) | `C *`, `C` the handle's C type; as a parameter, `const C *` where §4.3 says so | the C object it owns (§4.2, §4.3) |
| a function type (§3.6) | a pointer to a function of corresponding types | a callable C function (§3.6) |

A C type of the first and third rows corresponds to its primitive only on a target where it has the primitive's size, 32 or 64 bits: `int` on a target whose `int` is 32 bits, `long long` on one whose `long long` is 64. A C type of the second row corresponds to `@primitives$I32` only on a target where it is at most 32 bits. On any other target such a type corresponds to no Zane type and is reached through a shim (below). Likewise `float` and `double` correspond to `@primitives$F32` and `@primitives$F64` only on a target where they are IEEE 754 binary32 and binary64. A signed C type corresponds only on a target where it is two's complement, as Zane's integers are. An unsigned C type reads and writes the same bits as the signed primitive. So a `uint32_t` of `4294967295` is the `@primitives$I32` `-1`. A scalar primitive's **full-width C types** are those of its own size: the four 32-bit types of the first row for `@primitives$I32`, the 64-bit types for `@primitives$I64`, `float`, and `double`. Only they lay out as the primitive does, so only they appear inside an array, a list, or a `struct`.

A **C-compatible value type** is a value type declared with `struct` whose every field is a scalar primitive, an `@primitives$Array<T, n>` of one, or a C-compatible value type. Its C `struct` has the same number of fields, in the same order, each of a full-width C type of the Zane field's primitive, an array of one, or a `struct` corresponding in turn; their names need not match. The Zane type and the C `struct` then have the same layout, field for field.

```zane
type Vec3 = struct {
    x @primitives$F64;
    y @primitives$F64;
    z @primitives$F64;
}
```

```c
typedef struct { double x, y, z; } vec3;
```

A C type whose size differs between the targets the toolchain builds for, such as `long`, `unsigned long`, `wchar_t`, or `long double`, corresponds to no Zane type, and neither do `size_t`, `ptrdiff_t`, `intptr_t`, and `uintptr_t` on a target whose pointers are not 64 bits, so a C function that takes or returns one is reached through a C function of the package that converts it. A variadic C function is reached the same way.

### 3.4 Strings, arrays, and lists are lent for the call

A string, array, or list argument is a borrow ([`memory.md`](memory.md) §2.9), and C receives a read-only view of it that is valid until the call returns. `<zane_ffi.h>`, a header the toolchain provides to every package's C code, declares the two view types:

```c
typedef struct zane_bytes { const uint8_t *data; size_t size; } zane_bytes;
typedef struct zane_span  { const void    *data; size_t count; } zane_span;
```

A string's view holds its bytes, with no terminator. A list's view holds its elements, which are laid out as an array of `T`. C reads a view and never writes through it or keeps it after the call returns.

### 3.5 Results

A foreign verb's result is a scalar primitive, `@primitives$Bool`, `@primitives$Unit`, `@primitives$String`, a C-compatible value type, or a handle. An array, a list, and a function type cross into C only as parameters.

- A returned `zane_bytes` is a view of bytes C owns. The call copies them into a new string primitive before it returns, so the bytes stay valid after the C function has returned, until the copy is made: they are C memory that outlives the call, such as memory a handle owns, and never storage that ends when the C function returns. A view whose `size` is zero gives an empty string, whatever its `data`.
- A returned handle is a new roaming owner (§4.2). A foreign verb that returns one may declare `@primitives$Unit` as its abort type, and the call aborts when the C function returns a null pointer:

```zane
^Deflater?@primitives$Unit _deflater(level @primitives$I32) foreign "zane_deflater_new"
```

A foreign verb returning a handle without an abort type stops the program when the C function returns a null pointer. No other foreign verb declares an abort type: a C function signals failure through its result, and the package's Zane code turns that into an abort.

### 3.6 Function values cross as C functions

A parameter of function type ([`syntax.md`](syntax.md) §2.13) whose parameter types are scalar primitives, `@primitives$Bool`, or C-compatible value types, whose result type is one of those or `@primitives$Unit`, and which has no subject and no abort type, corresponds to a C function pointer of the corresponding types. C receives a function that calls the Zane function value it was passed.

C may call it any number of times before the foreign verb returns, and never after. What the function value reads and writes counts toward what the call reads and writes ([`effects.md`](effects.md) §5.2).

### 3.7 The declaration is checked against the interface header

The compiler checks each foreign verb against the declaration of its symbol in the package's interface headers, as the package's C code sees them when compiled for the build's target. It is a compile-time error, reported at the foreign verb, when:

- no interface header of the package declares the symbol as a function;
- the C function takes another number of parameters than the foreign verb passes (§3.2), or is variadic;
- a parameter's or the result's C type is not one §3.3 gives for the Zane type;
- a handle's pointer is `const` where §4.2 or §4.3 requires it writable, or writable where §4.3 requires it `const`;
- a C-compatible value type's layout differs from its C `struct`'s.

The check sees each target's C types, so a declaration that is correct for one target and wrong for another fails in the build for that other target.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#c-never-writes-zane-memory) — "C never writes Zane memory".

---

## 4. Foreign Handles

### 4.1 `#handle` and `#capability`

A **foreign handle** is a reference type that owns one C object. Its declaration names the C type of the objects it owns and the C function that releases one:

```zane
type Deflater = #handle("zane_deflater", "zane_deflater_free")
type Database = #capability("sqlite3", "zane_sqlite_close")
```

A type declared with `#handle` is a **data handle**: its C object holds state of the program's own, such as a compression stream or a parsed document. A type declared with `#capability` is a **foreign capability**: its C object stands for state outside the program, such as an open file, a database connection, or a socket, and reading or writing it is reading or writing capability-backed state ([`effects.md`](effects.md) §2.2, §5).

Both are reference types ([`types.md`](types.md) §2.1). A handle is moved rather than copied, is settled or roaming, and may be referenced through `&` like any reference type ([`memory.md`](memory.md) §2). It has no fields: C reaches the object it owns, and Zane reaches it only through foreign verbs.

The C type is a `struct` type, or a `typedef` name of one, that an interface header declares, completely or not. The release function is a C function of the package's interface headers that takes one non-`const` pointer to that type and returns `void`. A library's own cleanup function that returns a status, as SQLite's `sqlite3_close` returns an `int`, is reached through a C function of the package that calls it and returns `void`, such as the `zane_sqlite_close` above.

> **See also:** [`syntax.md`](syntax.md) §2.16 for the declaration form.

### 4.2 A handle is made by a foreign verb and released when it is destroyed

A handle is made only by a foreign verb that returns it (§3.5), as a writable `C *`, since the handle may later be a `mut` subject and is passed to its release function. It owns a non-null pointer to its C object from then on.

When a handle is destroyed ([`lifetimes.md`](lifetimes.md) §2), its release function is called with that pointer, exactly once, before the handle's storage is reused. A handle whose owner moves on keeps its object, and only the handle that is finally destroyed releases it.

Release functions are C code that runs while a scope drains, so their order is observable where the scope's other destruction is not. When one drain destroys several handles, their release functions run in the reverse of the order the handles were made. A handle made from another, such as a statement prepared on a connection, is made after it, so it is released before it (§4.4).

A settled place that holds a foreign handle, directly or through the owning fields of the objects it holds, is never overwritten: an assignment that would replace the handle there is a compile-time error. Overwriting a settled owner keeps references to it and gives them the replacement ([`memory.md`](memory.md) §2.2), which suits a Zane object but not a C object that another C object still points at, since the old one would be released under it. A handle in a settled place is therefore released only when its scope drains. A roaming owner, a list element, and a variant payload hold handles nothing references, so they are overwritten as any reference-type owner is.

### 4.3 Passing a handle reads it, and a `mut` subject writes it

A handle crosses into C as the pointer it owns. Where it stands in the foreign verb decides how C may use the object:

- As the subject of a `mut` foreign method, C receives a writable pointer, `C *`, and may change the object.
- In every other position, as the subject of a foreign method without `mut` or as another parameter, C receives a `const C *` and only reads the object.

A foreign verb writes at most one handle, its subject, because a `mut` method writes `this` and nothing else ([`effects.md`](effects.md) §4.2). A C function that changes two objects is reached through C functions of the package that each change one of them.

```zane
@primitives$String _deflate(this Deflater, input @primitives$String) mut foreign "zane_deflate"
@primitives$I64 _totalIn(this Deflater) foreign "zane_deflater_total_in"
```

```c
zane_bytes zane_deflate(zane_deflater *deflater, zane_bytes input);
int64_t zane_deflater_total_in(const zane_deflater *deflater);
```

### 4.4 A C object that depends on another

A C object that refers to another, such as a prepared statement and the connection it belongs to, needs that other object to outlive it. A handle expresses this through an `&` to the other held beside it, so that the store rule keeps the referenced handle alive at least as long ([`lifetimes.md`](lifetimes.md) §1.1):

```zane
type _RawStatement = #capability("sqlite3_stmt", "zane_sqlite_finalize")

type Statement = #struct {
    _raw _RawStatement;
    _database &Database;
}
```

The store rule allows the two to share a scope, and then both die in one drain; the reverse order of §4.2 releases the statement first, since it was made from the connection. The compiler knows nothing of how C objects refer to one another, so a binding that hands C a dependency without such a reference, or makes the dependent first, has to keep the dependency alive itself (§6).

> **Story:** [`stories/ffi.md`](../stories/ffi.md#c-never-writes-zane-memory) — "C never writes Zane memory".

---

## 5. Effects of Foreign Code

### 5.1 The system capability

`@runtime$System` is the capability for state outside the program that foreign code reaches: the file system, the clock, the environment, the network, and every other source of input or target of output a C library may use. It is a reference type with one instance, `@program$system`, created when the program starts. It cannot be constructed, and it is passed and stored like the program's console and runtime ([`effects.md`](effects.md) §6.6).

A foreign method whose subject is `@runtime$System` touches outside state that no foreign capability stands for. C does not receive the subject: it is the capability through which the call is made, not an argument.

```zane
@primitives$I64 _nowNanoseconds(this @runtime$System) foreign "zane_clock_now_ns"
@primitives$Unit _remove(this @runtime$System, path @primitives$String) mut foreign "zane_fs_remove"
```

### 5.2 What a foreign verb touches follows from its signature

What a foreign verb reads and writes outside the program is stated by its signature, and the compiler takes it from there ([`effects.md`](effects.md) §5.2):

| Foreign verb | Outside the program, it |
|---|---|
| A function, or a method on a data handle, taking no foreign capability | reads and writes nothing itself |
| A method without `mut` on `@runtime$System` or a foreign capability, or any foreign verb taking a foreign capability as a parameter | reads capability-backed state |
| A `mut` method on `@runtime$System` or a foreign capability | reads and writes capability-backed state |

A call also reads and writes whatever the function values it passes read and write (§3.6), as the compiler derives it for any call through a function value ([`effects.md`](effects.md) §5.2, §5.5). So a call that passes no capability, but passes a function value that writes the console, writes the console.

Calls then follow the ordinary rules ([`effects.md`](effects.md) §3, §4). A read needs a verb that reaches the capability. A write needs the capability writable: in the root package, the program's own `@program$system`; elsewhere, a capability reached through `this` of a `mut` method, stored from a writable source ([`effects.md`](effects.md) §6.2).

```zane
type Clock = #struct { _system &@runtime$System; }

Clock(system &@runtime$System) => init{_system = system;}

Int nanoseconds(this Clock) => Int(this._system:_nowNanoseconds())
```

A foreign verb on a data handle that is `mut` writes the handle, which is state of the program, and nothing outside it.

Destroying a foreign capability calls its release function (§4.2), which may change outside state, such as by closing a file. So it counts as a write to capability-backed state: it happens at run time, where the destruction falls, ordered as §5.6 orders every capability write. Destruction is not a write in the sense of [`effects.md`](effects.md) §4.1 ([`effects.md`](effects.md) §7.2), so the owner of a capability may let it be destroyed whether or not the verb may write it.

### 5.3 Making a foreign capability is a write

A foreign verb that returns a foreign capability **MUST** be a `mut` method whose subject is `@runtime$System` or a foreign capability. Opening a file or a connection is a write to outside state, and it makes a new capability writable by its owner, so only a verb that may write a capability may make one.

```zane
^Database?@primitives$Unit _open(this @runtime$System, path @primitives$String) mut foreign "zane_sqlite_open"
^_RawStatement?@primitives$Unit _prepare(this Database, sql @primitives$String) mut foreign "zane_sqlite_prepare"
```

A foreign verb that returns a data handle may be any foreign verb.

### 5.4 Compile-time evaluation of foreign calls

A call to a foreign verb that reads no capability-backed state computes its result from its arguments and from the data handles it is passed. It is therefore a computation like any other under [`effects.md`](effects.md) §5.3: when its arguments are known, the compiler may evaluate it at compile time. A computation in Zane that calls C folds through the call, and so does a data handle made, written, and destroyed within it.

Evaluating a foreign call at compile time changes nothing the program computes. Its result is the one the C function gives on the build's target, as the C code is compiled for that target (§2.2), and for a `release` dependency as the dependency's object was compiled (§2.6).

A call to a foreign verb that reads capability-backed state is never evaluated at compile time, and neither is anything that depends on its result. A call that writes capability-backed state happens at run time, at the point and in the order the program makes it.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#evaluating-c-as-the-target-runs-it) — "Evaluating C as the target runs it".

### 5.5 A foreign verb that touches outside state MUST say so

The compiler cannot see what a C function reads or writes. It takes each foreign verb's signature at its word (§5.2), and a foreign verb that takes no capability is taken to read and write nothing outside the program.

A foreign verb whose C function reads or writes state outside the program **MUST** be declared with the capability it touches: as a method on `@runtime$System` or on a foreign capability, `mut` if it writes. Outside state includes anything one run of the program can see differently from another: a file, the clock, the environment, a random source, a device, another process, and a C variable that another call changes.

A foreign verb that breaks this rule gives the program no meaning the spec defines. In particular, the compiler may:

- **evaluate the call at compile time**, so that the value the outside state held on the build machine, while the compiler ran, is built into the program, and every run of the program gives that value;
- **leave the call out** when its result is unused, so that what it writes is never written;
- **make one call where the program makes two** with the same arguments, or **reorder** the call, or **run it in parallel** with other work ([`concurrency.md`](concurrency.md) §2.1).

```zane
package env

@primitives$String home() foreign "zane_env_home"   // WRONG: reads the environment, and takes no capability
```

```zane
console!print(String(env$home()));
```

The call `env$home()` depends on nothing the program reads at run time, so an optimized build may evaluate it while it compiles. The program then prints the home directory of whoever built it, on every machine it runs on, and no diagnostic reports this. Declared with its capability, the call is a read, and it happens where the program runs:

```zane
package env

@primitives$String home(this @runtime$System) foreign "zane_env_home"
```

```zane
console!print(String(@program$system:env$home()));
```

The rule protects the binding's users, and only the binding's author can follow it.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#impurity-is-a-capability-not-a-comment) — "Impurity is a capability, not a comment".

### 5.6 Concurrency

A call to a foreign verb that touches no capability may run in parallel with other work, including other calls to the same C function, as any call that writes nothing its caller can see ([`concurrency.md`](concurrency.md) §2.1). Its C code is therefore called from more than one thread at a time.

Every call that reads or writes capability-backed state through `@runtime$System` or a foreign capability is taken to touch one external state, whatever capability it is made through, because C code behind two capabilities may share what it touches. Such calls are ordered by [`concurrency.md`](concurrency.md) §4.5: reads may run together, and a write is serialized with every other read and write.

A foreign handle is a reference type, so a spawned call never mutates one ([`concurrency.md`](concurrency.md) §4.2). Calls that only read a handle may run at the same time.

---

## 6. What Foreign Code Upholds

The compiler takes a foreign verb's signature as a description of its C function, and builds the rest of its guarantees on it. A C function reached through a foreign verb upholds each of the following. One that breaks any of them gives the program no meaning the spec defines.

- **It touches outside state only as declared** (§5.5).
- **It writes only what it is given writable**: the C object of its `mut` subject, and C memory that C owns. It writes no view (§3.4), and no handle passed as `const`.
- **It keeps nothing Zane lent it.** A view, and a function pointer for a function value (§3.6), are used only until the call returns.
- **It is safe to call from several threads at once** when it touches no capability (§5.6), and when it only reads a handle, alongside other reads of that handle.
- **It returns.** It leaves by returning to its caller, not by a long jump, an exception, or exiting the process. A C function that stops the program, such as by calling `abort`, stops it as a fault does.
- **It keeps its C objects consistent with their handles.** A release function is called once per object, as §4.2 states, and a C object stays alive while a C object that refers to it does (§4.4).
- **It treats a function value as a call.** A function value's C function is called with the types §3.6 gives, on any thread, while the foreign verb runs.

---

## 7. Language Comparisons

| Feature | Zane | Rust | Zig | Go (cgo) |
|---|---|---|---|---|
| C compiled by the language toolchain for any target | ✅ | ❌ (a build script and the `cc` crate) | ✅ | ⚠️ Note: a C cross compiler must be installed |
| A call into C can be evaluated at compile time | ✅ | ❌ | ❌ | ❌ |
| Effects of a C function visible in its signature | ✅ | ❌ | ❌ | ❌ |
| C signatures checked against headers | ✅ | ⚠️ Note: generated from them by `bindgen`, or written by hand unchecked | ✅ (`@cImport`) | ✅ |
| C objects released deterministically by their owner | ✅ | ⚠️ Note: by a hand-written `Drop` | ❌ (by `defer`) | ❌ |
| C may keep a pointer to the language's memory | ❌ | ✅ | ✅ | ⚠️ Note: only within cgo's pointer rules |

### 7.1 Zane vs. Rust (`extern` blocks)

Rust declares a C function in an `extern` block and calls it in `unsafe` code. The compiler sees only the declaration, so every call is opaque.

**Rust:**

```rust
extern "C" { fn crc32(crc: u64, data: *const u8, len: usize) -> u64; }
let c = unsafe { crc32(0, b"hello".as_ptr(), 5) }; // never computed at compile time
```

**Zane:**

```zane
@primitives$I64 _crc32(crc @primitives$I64, data @primitives$String) foreign "zane_crc32"
c @primitives$I64 = _crc32(@primitives$I64(0), @primitives$String("hello")); // may fold
```

| Difference | Rust | Zane |
|---|---|---|
| What the compiler knows of the C function | its declaration | its declaration and its body, compiled from source |
| Building the C | a `build.rs` program run on the build machine | the toolchain, from settings in the package's `c.coda` |
| Effects | none are stated; any call may do anything | stated by the signature, and taken as true |

### 7.2 Zane vs. Zig (`@cImport`)

Zig imports a C header and compiles C with its own toolchain for any target, which Zane's toolchain does as well. Zig exposes C's types to Zig code as they are, pointers included, and runs C only at run time.

| Difference | Zig | Zane |
|---|---|---|
| What crosses the boundary | C's own types, pointers included | the types of §3.3; a C object only behind a handle |
| C at compile time | never | when the call touches no capability |
| Releasing a C object | `defer` written at each use | the handle's release function, when the handle is destroyed |

---

## 8. Summary

| Concept | Rule |
|---|---|
| A package's C code | Its C sources (the `.c` files in its directory and those its `c.coda` lists) and its interface headers (the `.h` files in its directory) |
| Building | The toolchain compiles a package's C as C17 for the build's target; no build script runs; floating-point expressions are evaluated as written |
| `c.coda` | A hand-written file in the package's directory whose rows give its `source`, `include`, `define`, `archive`, and `system` settings, each for the targets a pattern matches |
| C symbols | Renamed into the package's placeholder namespace and rewritten with its version, so versions coexist |
| Foreign verb | A function or method whose body is a C function named by its symbol, checked against the package's interface headers |
| Boundary types | Scalar primitives, `Bool`, `Unit`, strings, arrays, and lists of scalars as read-only views, C-compatible value types, handles, and function values |
| Views | Valid for the call only; C never writes or keeps them; a returned `zane_bytes` is copied |
| Foreign handle | A `#handle` or `#capability` reference type owning one C object, released by its release function when it is destroyed; a settled place holding one is never overwritten, and one drain releases its handles in reverse making order |
| Writing a handle | Only as the subject of a `mut` foreign method; everywhere else C receives a `const` pointer |
| Null handle results | Abort with `Unit` when the verb declares it, and stop the program otherwise |
| `@runtime$System` | The capability for outside state; one instance, `@program$system`; never passed to C |
| Effects | No capability: nothing outside the program; a capability without `mut`: a read; with `mut`: a write |
| Making a capability | Only a `mut` method on `@runtime$System` or a foreign capability returns one |
| Compile-time evaluation | A foreign call that reads no capability-backed state may be evaluated at compile time, giving the target's result |
| Undeclared outside state | A foreign verb that touches outside state without its capability may be folded with the build machine's value, dropped, merged, reordered, or parallelized |
| Concurrency | Capability-free foreign calls may run in parallel; capability calls are ordered as one external state |
