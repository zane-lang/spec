
# Zane Dependency Management

## Overview

Zane has no central package registry. A package is identified by its full source URL — typically a GitHub or Codeberg repository URL. Two packages from different hosts can share the same short name without conflict because the URL, not the name, is the canonical identity.

Libraries are distributed as **pre-compiled object files** attached to a release. The Zane toolchain fetches and links them directly. The consumer never recompiles the library unless they explicitly opt in.

Versions are **exact release tags**. There is no implicit resolution, no semver range matching, and no global lockfile negotiation across packages. The exact tag is chosen by the user on the command line and written into the project manifest. What is in the manifest is what gets linked.

---

## Manifest

Every Zane project has a `zane.coda` file at its root. Dependencies are declared in a `deps` block:

```
deps [
    # key is the local alias used in import statements
    # url is the full package URL
    # version is the exact release tag
    math https://github.com/zane-lang/math vers1.0.1
    http https://github.com/zane-lang/http vers2.3.0
]
```

Each entry has three fields: a short **key** (the alias used in source code), the full **URL**, and the exact **version tag**. The user never edits `zane.coda` directly. All changes go through CLI commands:

```sh
zane add math https://github.com/zane-lang/math vers1.0.1
zane remove math
zane update math vers1.1.0
zane update           # re-fetches all deps and resolves each to its latest release
```

`zane add` fetches the package, installs it to the global packages directory, and appends the entry to the `deps` block. `zane remove` removes the entry and any packages no longer referenced by the project. `zane update` with a version argument pins the alias to the new tag; without arguments, it queries each package's releases and pins each to the latest.

---

## Package name rewriting at pull time

When a consumer runs `zane add`, the toolchain fetches the pre-compiled object file and rewrites every symbol that carries the `!` placeholder prefix — replacing `!` with the version tag:

```
!math$vec  →  vers1.0.1math$vec
!math$add  →  vers1.0.1math$add
```

This rewrite is performed using `llvm-objcopy --redefine-sym`. The rewritten object file is written to the global packages directory under the package URL and version (see [Global packages directory](#global-packages-directory)). From that point on, the version tag is permanently part of the symbol name embedded in the object file.

### The `!` placeholder convention

When a library author compiles their library, the Zane compiler emits all of the library's own symbols with a `!` prefix in place of the eventual version tag:

```
math$vec   (source)  →  !math$vec   (emitted into math.o)
math$add   (source)  →  !math$add   (emitted into math.o)
```

The `!` character is not a valid identifier character in Zane, so it cannot appear in any user-written name. It is reserved exclusively as this placeholder marker. At pull time the toolchain has everything it needs:

- the version tag (e.g. `vers1.0.1`)
- the object file, where every own symbol starts with `!`

It rewrites every symbol matching `!*` by replacing the leading `!` with the version tag. The search is unambiguous because `!` only appears as this placeholder — no other symbol in the object file starts with `!`.

For **transitive dependencies**, the own symbols of each library were already versioned when that library was pulled into the author's build environment. By the time a consumer pulls a transitive dependency, the `!`-prefixed symbols it emitted when compiled have already been rewritten to their final versioned form. No further rewriting of transitive symbols is needed.

---

## Global packages directory

Packages are installed to a **global location shared across all projects on the machine**:

```
~/.zane/packages/<mangled_url>/<mangled_version>/
```

The full URL is the identity key, not just the package name. URL components are mapped to a directory hierarchy using `/` as the separator — the same mangling scheme used by the Go module cache. The URL scheme separator `://` is collapsed to a single `/`:

```
https://github.com/zane-lang/math  →  https/github.com/zane-lang/math
```

Capital letters are escaped with `!` followed by the lowercase equivalent (e.g. `MyLib` → `!my!lib`), matching Go's path-escaping rules for case-insensitive filesystems.

Version tags follow the same escaping rule for capital letters. Tags that contain characters illegal on the local filesystem (e.g. `:`, `*`, `?`, `<`, `>`, `|`, `\`, `"`, or a raw `/`) cause `zane add` to abort with a pull-time error. These characters cannot be represented safely in a directory name on all supported platforms.

A typical installed layout looks like:

```
~/.zane/packages/
    https/
        github.com/
            zane-lang/
                math/
                    vers1.0.1/
                        math.o
                        zane.coda
                    vers2.0.0/
                        math.o
                        zane.coda
                http/
                    vers2.3.0/
                        http.o
                        zane.coda
```

The shared packages directory means the same package version is never downloaded or prefixed more than once across all projects on the machine. If two projects both depend on `vers1.0.1` of `math`, the second `zane add` is a no-op — the already-prefixed object file is reused.

---

## Import syntax and compiler substitution

In source code, packages are imported by their manifest key:

```zane
import math
import http
```

And used with the `$` separator:

```zane
math$vec(1.0, 2.0)
http$get("https://example.com")
```

At compile time, the compiler looks up each alias in the `deps` table and substitutes the fully versioned form throughout the compilation unit. The programmer never writes version strings in source:

| Source form | Compiled form |
|---|---|
| `import math` | resolves to `vers1.0.1math` internally |
| `math$vec` | emitted as `vers1.0.1math$vec` in the object file |
| `import http` | resolves to `vers2.3.0http` internally |
| `http$get` | emitted as `vers2.3.0http$get` in the object file |

Consumer code is compiled with full knowledge of each dependency's version (from `zane.coda`), so the compiler emits fully versioned symbol references directly. The `!` placeholder prefix only appears in library object files — it is the marker used when a library is compiled without a version, so the toolchain can inject one at pull time.

**Enforcement:** Every name used in an `import` statement must be a key in the `deps` table. Writing `import vers1.0.1math` directly in source is a compile error — the string `vers1.0.1math` is not a manifest key. There is no way to bypass the manifest and reference a versioned symbol directly. This ensures the manifest is always the single source of truth for version resolution.

---

## Transitive dependencies

Libraries declare their own dependencies in their own `zane.coda`. When a package is pulled, the toolchain reads its manifest and **recursively installs all transitive dependencies** into the global packages directory before the package itself is considered ready.

When a library is compiled, the compiler emits its own symbols with the `!` placeholder prefix (e.g. `!math$vec`). References to other packages it depends on are emitted as fully versioned symbols, because those packages were already pulled and versioned in the author's build environment. By the time a consumer pulls the library, the `!`-prefixed own symbols are rewritten with the version tag; the already-versioned transitive references are left unchanged.

Because versioned symbols are distinct strings, two packages that depend on **different versions of the same library** coexist cleanly. If `plot` requires `vers2.0.0math` and `http` requires `vers1.5.0math`, both are installed, and their symbols (`vers2.0.0math$*` and `vers1.5.0math$*`) are entirely distinct. No conflict, no resolution algorithm, no diamond problem.

---

## Multiple version coexistence

Because version tags are embedded in symbol names, multiple versions of the same library can coexist in a single binary without any conflict:

```
vers1.0.1math$vec    // from one dep's transitive chain
vers2.0.0math$vec    // from another dep's transitive chain
```

These are distinct symbols. The linker sees no ambiguity. There is no implicit global resolution that could vary between machines or build environments.

This also makes builds more deterministic: the exact version is always explicit in every symbol reference, and the resolved version is pinned in `zane.coda`. Two developers with the same manifest always link the same symbols.

Tradeoff: if a project transitively pulls in several major versions of a large library, binary size grows proportionally. There is no deduplication across versions — each version is a separate set of symbols. This is an intentional tradeoff: correctness and determinism are prioritised over binary size optimisation.

---

## Platform artifacts

Pre-compiled object files are platform-specific. A release typically attaches one object file per target triple (e.g. `x86_64-linux`, `aarch64-macos`, `x86_64-windows`). The toolchain selects the correct artifact based on the host target.

Library authors are expected to build release artifacts using CI. A typical pattern:

```
# build.yml (GitHub Actions)
strategy:
  matrix:
    target: [x86_64-linux, aarch64-macos, x86_64-windows]

steps:
  - run: zig build-lib -target ${{ matrix.target }} math.zane -femit-bin=math-${{ matrix.target }}.o
  - run: gh release upload $TAG math-${{ matrix.target }}.o
```

The Zane compiler uses the [Zig](https://ziglang.org) toolchain for cross-compilation. A library built on a Linux CI machine can produce correct object files for all supported targets without additional setup.

### Trust and source-compile opt-in

By default, the toolchain uses the pre-compiled object file from the release. A consumer who does not trust the distributor can opt in to **compiling the package from source** instead:

```sh
zane add math https://github.com/zane-lang/math vers1.0.1 --from-source
```

This clones the repository at the specified tag, compiles it locally, and installs the result to the packages directory in place of the pre-built artifact. The output is functionally identical to the pre-built object after symbol prefixing. The `--from-source` flag is a trust/verification escape hatch, not a normal workflow.

---

## Build flow

The end-to-end flow from library authorship to consumer compilation:

```
1. Library author writes and compiles the library.
   Compiler emits !math$vec, !math$add, ... using ! as a version placeholder prefix.

2. Author creates a GitHub/Codeberg release with a version tag (e.g. vers1.0.1).
   Author attaches the compiled object file(s) to the release.

3. Consumer runs: zane add math https://github.com/zane-lang/math vers1.0.1
   Toolchain fetches math.o from the vers1.0.1 release.
   Toolchain runs: llvm-objcopy --redefine-sym !math$vec=vers1.0.1math$vec ...
   (Replaces the leading ! with the version tag on every !-prefixed symbol.)
   Prefixed object file is written to:
       ~/.zane/packages/https/github.com/zane-lang/math/vers1.0.1/math.o
   Manifest entry is appended to zane.coda.
   Transitive deps from math's zane.coda are fetched recursively.
   (Transitive dep object files have their own !-prefixed symbols rewritten when each is pulled.)

4. Consumer writes source code:
       import math
       let v = math$vec(1.0, 2.0)

5. Consumer runs: zane build
   Compiler reads zane.coda, resolves math → vers1.0.1.
   Compiler substitutes: math$vec → vers1.0.1math$vec throughout the IR.
   Linker links against ~/.zane/packages/https/github.com/zane-lang/math/vers1.0.1/math.o.
   Binary is produced.
```

---

## Summary table

| Design decision | Rationale |
|---|---|
| No central registry | Eliminates a single point of failure and control; URL = identity |
| Exact version pinning | Deterministic builds; no surprise upgrades; explicit in manifest |
| Symbol prefixing at pull time | `!` placeholder in library object files is replaced with the version tag; unambiguous because `!` is not a valid identifier character |
| Transitive deps reference versioned symbols at compile time | Library authors compile against already-pulled (versioned) deps; only own `!`-prefixed symbols need rewriting at each pull |
| Global shared packages directory | No duplicate downloads or prefixing; shared across all projects on the machine |
| Go-style URL and tag path mangling | `/` as subdir separator mirrors Go module cache; capital letters escaped with `!`; illegal chars error at pull time |
| Version in symbol name | Multiple versions coexist in one binary; no linker conflicts |
| Manifest key enforcement | No way to bypass the manifest; version strings stay out of source code |
| Pre-compiled object files | Fast dependency installation; no recompilation on the consumer side |
| Source-compile opt-in | Trust escape hatch for security-conscious consumers |
| Transitive deps auto-installed | Consumer does not need to enumerate indirect deps |
| Per-library version namespace | Diamond deps resolve trivially; no global version negotiation |
