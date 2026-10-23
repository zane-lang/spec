# Zane C Interface

This document specifies how a Zane package calls C: the C code a package holds and how the toolchain builds it, foreign verbs and the types that cross into C, foreign handles, and what foreign code is taken to read and write.

> **See also:** [`effects.md`](effects.md) §5 and §6 for capability access and compile-time evaluation. [`memory.md`](memory.md) §2.9 for passing modes. [`dependencies.md`](dependencies.md) §3 and §6 for release archives and symbol rewriting. [`syntax.md`](syntax.md) §2.16 and §3.11 for the declaration forms.

> **Story:** [`stories/ffi.md`](../stories/ffi.md) tells the story behind this design.

---

## 1. Overview

A package may hold C code beside its Zane source. The toolchain compiles that C for the build's target, links it into the package, and lets the package's own declarations call it. A call into C is an ordinary call in Zane: it has a Zane signature, Zane passing modes, and the effects that signature states.

- **`The toolchain builds the C`.** A package's C sources are compiled by the toolchain's own C compiler, for whatever target the build is for, with settings from a `c.coda` file in the package's directory. No build script runs.
- **`Foreign verbs`.** A function or method whose body is a C function, named by its C symbol. Its Zane signature is checked against the C declaration in the package's interface headers.
- **`C never writes Zane memory`.** Zane values cross into C as values or as read-only views for the duration of the call. State that C keeps lives in C memory, reached from Zane through a **foreign handle**, which releases it when it is destroyed and replaces it in place when it is overwritten.
- **`Outside state is a capability`.** A package's C code offers its reach outside the program as a C global, which the root package declares as a **foreign variable** and passes on. A foreign verb that is a method on such a capability reads or writes outside state; any other foreign verb is taken to read and write nothing outside the program, so its calls fold at compile time like any other computation.
- **`The declaration is trusted`.** The compiler cannot see what C code touches. A foreign verb declared without a capability that in fact reads outside state is evaluated at compile time, and the value the build machine produced is built into the program (§5.5).

> **Story:** [`stories/ffi.md`](../stories/ffi.md#a-c-interface-that-keeps-compile-time-evaluation) — "A C interface that keeps compile-time evaluation".

---

## 2. A Package's C Code

### 2.1 C sources and interface headers

Every package may hold C code: a library, program, or test package, and a subpackage. Its C code is:

- its **C sources**: the `.c` files directly in the package's directory, together with any listed in its `c.coda` (§2.3);
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

A path **MUST** stay inside the package's directory, so that everything a package's C build reads moves with the package; one that is absolute or leaves the package's directory through `..` is an error. C that two packages share is vendored into each, or kept in one package whose foreign verbs the other calls. A `source` pattern that matches no file in a build it applies to is an error. The rows apply in the order they are written.

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

A foreign verb's parameters are the C function's parameters, in order. A method's subject is the first of them.

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
| a foreign handle `H` (§4) | `C *`, `C` the handle's C type; as a parameter, `const C *` where §4.4 says so | the C object it refers to (§4.2, §4.4) |
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
- A returned handle is a new roaming owner (§4.2), of a type with a free function. A foreign verb that returns one may declare `@primitives$Unit` as its abort type, and the call aborts when the C function returns a null pointer:

```zane
^Deflater?@primitives$Unit _deflater(level @primitives$I32) foreign "zane_deflater_new"
```

A foreign verb returning a handle without an abort type stops the program when the C function returns a null pointer. No other foreign verb declares an abort type: a C function signals failure through its result, and the package's Zane code turns that into an abort.

### 3.6 Function values cross as C functions

A parameter of function type ([`syntax.md`](syntax.md) §2.13) whose parameter types are scalar primitives, `@primitives$Bool`, or C-compatible value types, whose result type is one of those or `@primitives$Unit`, and which has no subject and no abort type, corresponds to a C function pointer of the corresponding types. C receives a function that calls the Zane function value it was passed.

C may call it any number of times before the foreign verb returns, and never after. What the function value reads and writes counts toward what the call reads and writes ([`effects.md`](effects.md) §5.2).

C may call a function value that touches no capability from several threads at once. A function value that reads or writes capability-backed state is called one call at a time: C does not start a call to it while another is running. That keeps its accesses ordered against each other as [`concurrency.md`](concurrency.md) §4.5 orders every access to capability-backed state, since the compiler orders the foreign call as a whole and sees none of the calls C makes inside it.

### 3.7 The declaration is checked against the interface header

The compiler checks each foreign verb against the declaration of its symbol in the package's interface headers, as the package's C code sees them when compiled for the build's target. It is a compile-time error, reported at the foreign verb, when:

- no interface header of the package declares the symbol as a function;
- the C function takes a different number of parameters than the foreign verb passes (§3.2), or is variadic;
- a parameter's or the result's C type is not one §3.3 gives for the Zane type;
- a handle's pointer is `const` where §4.2 or §4.4 requires it writable, or writable where §4.4 requires it `const`;
- a handle stands where §4.4 does not allow it;
- a C-compatible value type's layout differs from its C `struct`'s.

The check sees each target's C types, so a declaration that is correct for one target and wrong for another fails in the build for that other target.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#c-never-writes-zane-memory) — "C never writes Zane memory".

---

## 4. Foreign Handles

### 4.1 `#handle`

A **foreign handle** is a reference type whose value refers to one C object. Its declaration names the C type of those objects and, where Zane owns them, the C functions that replace and release one:

```zane
type Fs       = #handle{ cType = "zane_fs_t"; }
type File     = #handle{ cType = "zane_file_t"; free = "zane_fclose"; }
type Deflater = #handle{ cType = "zane_deflater_t"; set = "zane_deflater_set"; free = "zane_deflater_free"; }
```

- **`cType`** is a `struct` type, or a `typedef` name of one, that an interface header declares, completely or not.
- **`free`** is the **free function**: a C function of the package's interface headers that takes one non-`const` pointer to the C type and returns `void`. It releases an object when its handle is destroyed (§4.2). A library's own cleanup function that returns a status, as SQLite's `sqlite3_close` returns an `int`, is reached through a C function of the package that calls it and returns `void`.
- **`set`** is the **setter**: a C function of the package's interface headers that takes two non-`const` pointers to the C type and returns `void`. It replaces an object in place when a settled handle is overwritten (§4.3). A handle type with a setter has a free function.

A handle type without a free function is a **foreign capability**. Its values refer to C globals, are made only by foreign variables (§5.1), and stand for state outside the program. A handle type with a free function owns its C object, and is one of two kinds according to the foreign verbs that make it (§4.2):

- an **outside handle**, made by a method on a foreign capability, whose C object is part of the state outside the program that the capability reaches, such as an open file or a database connection;
- a **data handle**, made by any other foreign verb, whose C object holds state of the program's own, such as a compression stream or a parsed document.

Every handle type is a reference type ([`types.md`](types.md) §2.1). A handle is moved rather than copied, is settled or roaming, and may be referenced through `&` like any reference type ([`memory.md`](memory.md) §2). It has no fields: C reaches the object it refers to, and Zane reaches it only through foreign verbs.

> **See also:** [`syntax.md`](syntax.md) §2.16 for the declaration form.

### 4.2 A handle is made by a foreign verb and released when it is destroyed

A handle with a free function is made only by a foreign verb that returns it (§3.5), as a writable `C *`, since it is later passed to its free function. It owns a non-null pointer to its C object from then on. A foreign verb never returns a foreign capability.

The foreign verbs that return one handle type are all methods on a foreign capability or none of them are. A type that one makes and another does not is a compile-time error, reported at the type's declaration, because it would be outside state and program state at once.

When a handle is destroyed ([`lifetimes.md`](lifetimes.md) §2), its free function is called with that pointer, exactly once, before the handle's storage is reused. A handle whose owner moves on keeps its object, and only the handle that is finally destroyed releases it. A foreign capability has no free function, and destroying one releases nothing.

Free functions are C code that runs while a scope drains, so their order is observable where the scope's other destruction is not. When one drain destroys several handles, their free functions run in the reverse of the order the handles were made. A handle made from another, such as a statement prepared on a connection, is made after it, so it is released before it (§4.5).

C keeps each object at the address its handle holds until the free function releases it, and other C objects may hold that address. Zane therefore never moves a C object, and replaces one only through its setter.

### 4.3 A settled handle is overwritten through its setter

Overwriting a settled owner writes the replacement at the same address, so references to it see the replacement ([`memory.md`](memory.md) §2.2). A foreign handle in a settled place, directly or through the owning fields of the objects that place holds, is overwritten the same way through its setter: the setter receives the pointer the place holds and the replacement's pointer, and leaves the object at the first address holding the replacement.

```c
void zane_deflater_set(zane_deflater_t *dst, zane_deflater_t *src);
```

The setter releases the object `dst` held, moves `src`'s object into `dst`, and frees what remains of `src`. The replacement handle is consumed by the call, and its free function is not called. The object keeps its address, so a C object that holds that address reaches the replacement.

The handle keeps its place in the release order of §4.2 too. Handles made from its object hold that address and now reach the replacement, so they are still released before it. A replacement made from a handle that is younger than the one it replaces is therefore released after that handle, and a binding whose replacement depends on such an object has to keep the dependency alive itself, as it must for any dependency the order does not cover (§4.5).

One overwrite can reach several handles, such as a struct that holds a connection and a statement prepared on it. Their setters run in the reverse of the order the handles were made, the order a drain releases them in (§4.2), so a handle's old object is released before the old object it was made from.

An assignment that would overwrite a handle in a settled place is a compile-time error when the handle's type has no setter. A foreign capability has none, so a settled place holding one is never overwritten. A roaming owner, a list element, and a variant payload are overwritten as any reference-type owner is: the old handle is destroyed (§4.2) and the new one takes its place.

```zane
d Deflater = _deflater(@primitives$I32(6));
d = _deflater(@primitives$I32(9));          // calls zane_deflater_set

log fs$File = files!fs$open("a.log");
log = files!fs$open("b.log");             // ERROR: File has no setter
```

> **Story:** [`stories/ffi.md`](../stories/ffi.md#replacing-a-handle-in-place) — "Replacing a handle in place".

### 4.4 Passing a handle

A handle crosses into C as the pointer it holds. Where it stands in the foreign verb decides how C may use the object:

- **A foreign capability** stands, in a foreign verb, only as the subject of a foreign method (§5.1). C receives `C *` when the method is `mut`, and `const C *` otherwise.
- **An outside handle** stands, in a foreign verb, only as a parameter other than the subject of a method on a foreign capability, or as its result. C receives `C *` when the method is `mut`, and `const C *` otherwise: the object is part of the outside state the capability reaches, so a write through the capability may change it.
- **A data handle** stands anywhere. C receives `C *` when it is the subject of a `mut` foreign method, and `const C *` in every other position, where it only reads the object.

A data handle is written only as the subject of a `mut` method, because a `mut` method writes `this` and no other state of the program ([`effects.md`](effects.md) §4.2). A C function that changes two data handles is reached through C functions of the package that each change one of them.

```zane
@primitives$String _deflate(this Deflater, input @primitives$String) mut foreign "zane_deflate"
@primitives$I64 _totalIn(this Deflater) foreign "zane_deflater_total_in"

@primitives$Unit _write(this Fs, file File, data @primitives$String) mut foreign "zane_fwrite"
```

```c
zane_bytes zane_deflate(zane_deflater_t *deflater, zane_bytes input);
int64_t zane_deflater_total_in(const zane_deflater_t *deflater);

void zane_fwrite(zane_fs_t *fs, zane_file_t *file, zane_bytes data);
```

### 4.5 A C object that depends on another

A C object that refers to another, such as a prepared statement and the connection it belongs to, needs that other object to outlive it. A handle expresses this through an `&` to the other held beside it, so that the store rule keeps the referenced handle alive at least as long ([`lifetimes.md`](lifetimes.md) §1.1):

```zane
type Sqlite        = #handle{ cType = "zane_sqlite_t"; }
type Database      = #handle{ cType = "sqlite3"; free = "zane_sqlite_close"; }
type _RawStatement = #handle{ cType = "sqlite3_stmt"; free = "zane_sqlite_finalize"; }

type Statement = #struct {
    _raw _RawStatement;
    _database &Database;
}
```

The store rule allows the two to share a scope, and then both die in one drain; the reverse order of §4.2 releases the statement first, since it was made from the connection. A settled `Database` is never overwritten, since it has no setter, so the connection a statement holds stays the one it was prepared on. The compiler knows nothing of how C objects refer to one another, so a binding that hands C a dependency without such a reference, or makes the dependent first, has to keep the dependency alive itself (§6).

> **Story:** [`stories/ffi.md`](../stories/ffi.md#c-never-writes-zane-memory) — "C never writes Zane memory".

---

## 5. Effects of Foreign Code

### 5.1 Foreign capabilities come from C globals

A package whose C code reaches state outside the program offers that reach as a C global, of a C type its interface header declares and a foreign capability names:

```c
// fs.h
typedef struct zane_fs_t   zane_fs_t;
typedef struct zane_file_t zane_file_t;

extern zane_fs_t zane_fs;

zane_file_t *zane_fopen(zane_fs_t *fs, zane_bytes path);
void zane_fwrite(zane_fs_t *fs, zane_file_t *file, zane_bytes data);
void zane_fclose(zane_file_t *file);
```

```zane
package fs

type Fs   = #handle{ cType = "zane_fs_t"; }
type File = #handle{ cType = "zane_file_t"; free = "zane_fclose"; }

^File open(this Fs, path @primitives$String) mut foreign "zane_fopen"
@primitives$Unit write(this Fs, file File, data @primitives$String) mut foreign "zane_fwrite"
```

A **foreign variable** declares a symbol whose value is a handle to such a global. It is written like a symbol declaration, with `foreign` and the global's symbol in place of an initializer:

```zane
Unit main() {
    files fs$Fs foreign "zane_fs";
    log fs$File = files!fs$open("app.log");
    files!fs$write(log, "started\n");
}
```

The symbol is a settled owner of a foreign capability. The global is looked up in the package that declares the capability's type: its C code defines it with external linkage, and one of its interface headers declares it with the type's C type. It is a compile-time error, reported at the declaration, when no such global exists, its C type is another, or it is thread-local (`_Thread_local`), since a thread-local global is a separate object on each thread and a call that reaches it may run on any thread.

Only the root package declares foreign variables ([`packages.md`](packages.md) §6.1). A foreign variable in any other package is a compile-time error. Every other package reaches a foreign capability only through one passed to it, as it reaches the program's console ([`effects.md`](effects.md) §6.6), so a package that touches outside state shows it in what it receives. Two foreign variables that name one global are two handles to one C object, and since a foreign capability has no free function or setter, neither releases or replaces it.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#capabilities-come-from-c) — "Capabilities come from C".

### 5.2 What a foreign verb touches follows from its signature

What a foreign verb reads and writes outside the program is stated by its signature, and the compiler takes it from there ([`effects.md`](effects.md) §5.2):

| Foreign verb | Outside the program, it |
|---|---|
| A function, or a method on a data handle | reads and writes nothing itself |
| A method without `mut` on a foreign capability | reads capability-backed state |
| A `mut` method on a foreign capability | reads and writes capability-backed state |

The state an outside handle refers to is capability-backed state, and a foreign verb reaches it only as a method on a foreign capability (§4.4). A foreign verb on a data handle that is `mut` writes the handle, which is state of the program, and nothing outside it.

A call also reads and writes whatever the function values it passes read and write (§3.6), as the compiler derives it for any call through a function value ([`effects.md`](effects.md) §5.2, §5.5). So a call that passes no capability, but passes a function value that writes the console, writes the console.

Calls then follow the ordinary rules ([`effects.md`](effects.md) §3, §4). A read needs a verb that reaches the capability. A write needs the capability writable: in the root package, a foreign variable it declares; elsewhere, a capability reached through `this` of a `mut` method, stored from a writable source ([`effects.md`](effects.md) §6.2).

```zane
package time

type Clock = #handle{ cType = "zane_clock_t"; }

@primitives$I64 _nowNanoseconds(this Clock) foreign "zane_clock_now_ns"

type Timer = #struct { _clock &Clock; }

Timer(clock &Clock) => init{_clock = clock;}

Int nanoseconds(this Timer) => Int(this._clock:_nowNanoseconds())
```

Destroying an outside handle calls its free function (§4.2), which may change outside state, such as by closing a file, and overwriting one calls its setter (§4.3). Both count as writes to capability-backed state: they happen at run time, where the destruction or the assignment falls, ordered as §5.6 orders every capability write. Destruction is not a write in the sense of [`effects.md`](effects.md) §4.1 ([`effects.md`](effects.md) §7.2), so the owner of an outside handle may let it be destroyed whether or not the verb may write the capability that made it.

### 5.3 Making an outside handle is a write

A foreign verb that returns an outside handle **MUST** be a `mut` method on a foreign capability. Opening a file or a connection is a write to outside state, and it gives its owner a handle to that state, so only a verb that may write the capability may make one.

```zane
^Database?@primitives$Unit _open(this Sqlite, path @primitives$String) mut foreign "zane_sqlite_open"
^_RawStatement?@primitives$Unit _prepare(this Sqlite, database Database, sql @primitives$String) mut foreign "zane_sqlite_prepare"
```

A foreign verb that returns a data handle is any foreign verb that is not a method on a foreign capability.

### 5.4 Compile-time evaluation of foreign calls

A call to a foreign verb that reads no capability-backed state computes its result from its arguments and from the data handles it is passed. It is therefore a computation like any other under [`effects.md`](effects.md) §5.3: when its arguments are known, the compiler may evaluate it at compile time. A computation in Zane that calls C folds through the call, and so does a data handle made, written, and destroyed within it.

Two computations that call C are never evaluated at compile time, because what they give is a fact of the running process rather than of their arguments:

- one whose result depends on where an object is stored: the numeric value of an address, such as a pointer converted to an integer, or the order of two different objects' addresses;
- one that ends with a data handle it made still alive, since that handle's C object exists only in the process that made it.

Evaluating a foreign call at compile time changes nothing the program computes. Its result is the one the C function gives on the build's target, as the C code is compiled for that target (§2.2), and for a `release` dependency as the dependency's object was compiled (§2.6).

A call to a foreign verb that reads capability-backed state is never evaluated at compile time, and neither is anything that depends on its result. A call that writes capability-backed state happens at run time, at the point and in the order the program makes it.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#evaluating-c-as-the-target-runs-it) — "Evaluating C as the target runs it".

### 5.5 A foreign verb that touches outside state MUST say so

The compiler cannot see what a C function reads or writes. It takes each foreign verb's signature at its word (§5.2), and a foreign verb that is not a method on a foreign capability is taken to read and write nothing outside the program.

A foreign verb whose C function reads or writes state outside the program **MUST** be declared as a method on the foreign capability that reaches that state, `mut` if it writes. Outside state includes anything one run of the program can see differently from another: a file, the clock, the environment, a random source, a device, another process, and a C variable that another call changes.

A data handle's free function and setter **MUST** read and write no outside state. They are named by the handle's type rather than declared as verbs, so no capability can mark them, and they run where the program destroys or overwrites the handle, which a computation evaluated at compile time may do (§5.4). An outside handle's free function and setter may touch outside state, since running them counts as a capability write (§5.3).

A foreign verb, free function, or setter that breaks these rules gives the program no meaning the spec defines. In particular, the compiler may:

- **evaluate the call at compile time**, so that the value the outside state held on the build machine, while the compiler ran, is built into the program, and every run of the program gives that value;
- **leave the call out** when its result is unused, so that what it writes is never written;
- **make one call where the program makes two** with the same arguments, or **reorder** the call, or **run it in parallel** with other work ([`concurrency.md`](concurrency.md) §2.1).

A package that counts with a C variable shows how:

```c
static int64_t count;

int64_t zane_counter_next(void) { return ++count; }
```

```zane
package counter

@primitives$I64 next() foreign "zane_counter_next"   // WRONG: changes a C variable the next call reads, and is no method on a capability
```

```zane
console!print(Int(counter$next()));
console!print(Int(counter$next()));
console!print(Int(counter$next()));
```

Each call `counter$next()` depends on nothing the program reads at run time, so an optimized build may evaluate each one while it compiles, from the variable's first value. The program then prints `1` three times where an unoptimized build prints `1`, `2`, and `3`, and no diagnostic reports this. The same holds for a C function that reads the environment or the clock, whose fold builds the value the build machine saw into the program. Declared as a method on a capability, the call is a write, and it happens where the program runs, in the order the program makes it:

```c
typedef struct zane_counter_t { int64_t count; } zane_counter_t;

zane_counter_t zane_counter;

int64_t zane_counter_next(zane_counter_t *counter) { return ++counter->count; }
```

```zane
package counter

type Counter = #handle{ cType = "zane_counter_t"; }

@primitives$I64 next(this Counter) mut foreign "zane_counter_next"
```

```zane
Unit main() {
    counter counter$Counter foreign "zane_counter";
    console!print(Int(counter!counter$next()));
}
```

The rule protects the binding's users, and only the binding's author can follow it.

> **Story:** [`stories/ffi.md`](../stories/ffi.md#impurity-is-a-capability-not-a-comment) — "Impurity is a capability, not a comment".

### 5.6 Concurrency

A call to a foreign verb that touches no capability may run in parallel with other work, including other calls to the same C function, as any call that writes nothing its caller can see ([`concurrency.md`](concurrency.md) §2.1). Its C code is therefore called from more than one thread at a time.

Every call that reads or writes capability-backed state through a foreign capability is taken to touch one external state, whatever capability it is made through, because C code behind two capabilities may share what it touches. Such calls, and the destruction and overwriting of outside handles (§5.2), are ordered by [`concurrency.md`](concurrency.md) §4.5: reads may run together, and a write is serialized with every other read and write.

A foreign handle is a reference type, so a spawned call never mutates one ([`concurrency.md`](concurrency.md) §4.2). Calls that only read a handle may run at the same time.

---

## 6. What Foreign Code Upholds

The compiler takes a foreign verb's signature as a description of its C function, and builds the rest of its guarantees on it. A C function reached through a foreign verb upholds each of the following. One that breaks any of them gives the program no meaning the spec defines.

- **It touches outside state only as declared** (§5.5).
- **It writes only what it is given writable**: the C object of its `mut` subject, the outside handles a `mut` method on a foreign capability is passed, and C memory that C owns. It writes no view (§3.4), and no handle passed as `const`.
- **It keeps nothing Zane lent it.** A view, and a function pointer for a function value (§3.6), are used only until the call returns.
- **It is safe to call from several threads at once** when it touches no capability (§5.6), and when it only reads a handle, alongside other reads of that handle.
- **It returns.** It leaves by returning to its caller, not by a long jump, an exception, or exiting the process. A C function that stops the program, such as by calling `abort`, stops it as a fault does.
- **It keeps its C objects consistent with their handles.** A free function is called once per object, as §4.2 states. A C object stays at the address its handle holds until its free function releases it (§4.2), and stays alive while a C object that refers to it does (§4.5). A setter leaves the object at the address it was given, holding the replacement (§4.3).
- **It treats a function value as a call.** A function value's C function is called with the types §3.6 gives, on any thread, while the foreign verb runs, and one call at a time when the function value touches a capability.

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
| Releasing a C object | `defer` written at each use | the handle's free function, when the handle is destroyed |

---

## 8. Summary

| Concept | Rule |
|---|---|
| A package's C code | Its C sources (the `.c` files in its directory and those listed in its `c.coda`) and its interface headers (the `.h` files in its directory) |
| Building | The toolchain compiles a package's C as C17 for the build's target; no build script runs; floating-point expressions are evaluated as written |
| `c.coda` | A hand-written file in the package's directory whose rows give its `source`, `include`, `define`, `archive`, and `system` settings, each for the targets a pattern matches |
| C symbols | Renamed into the package's placeholder namespace and rewritten with its version, so versions coexist |
| Foreign verb | A function or method whose body is a C function named by its symbol, checked against the package's interface headers |
| Boundary types | Scalar primitives, `Bool`, `Unit`, strings, arrays, and lists of scalars as read-only views, C-compatible value types, handles, and function values |
| Views | Valid for the call only; C never writes or keeps them; a returned `zane_bytes` is copied |
| Foreign handle | A `#handle{ cType; set; free; }` reference type referring to one C object; its free function releases the object when the handle is destroyed, and one drain releases its handles in the reverse of the order they were made |
| Overwriting a handle | A settled handle is overwritten only through its setter, which keeps the C object's address and the handle's place in the release order; one overwrite runs its setters in the reverse of the order the handles were made; a type without one is never overwritten in a settled place |
| Writing a handle | A data handle only as the subject of a `mut` foreign method; an outside handle in a `mut` method on a foreign capability; everywhere else C receives a `const` pointer |
| Null handle results | Abort with `Unit` when the verb declares it, and stop the program otherwise |
| Foreign capability | A handle type without a free function, referring to a C global; made only by a foreign variable |
| Foreign variable | `name T foreign "cGlobal"`, declared only in the root package, giving a handle to a global of `T`'s package that is not thread-local |
| Outside and data handles | A handle made by a method on a foreign capability is an outside handle, part of capability-backed state; any other is a data handle, program state |
| Effects | A method on a foreign capability without `mut`: a read; with `mut`: a write; any other foreign verb: nothing outside the program |
| Making an outside handle | Only a `mut` method on a foreign capability returns one |
| Compile-time evaluation | A foreign call that reads no capability-backed state may be evaluated at compile time, giving the target's result, unless the computation observes an address or leaves a data handle it made alive |
| Undeclared outside state | A foreign verb that touches outside state without its capability, or a data handle's free function or setter that touches it at all, may be folded with the build machine's value, dropped, merged, reordered, or parallelized |
| Concurrency | Capability-free foreign calls may run in parallel; capability calls are ordered as one external state |
