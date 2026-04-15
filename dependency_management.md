# Zane Dependency Management

## Overview

Zane's dependency management system has no central registry. A package is identified by its full repository URL — a GitHub or Codeberg URL is the canonical package identity. No account, approval process, or central index is involved. If a repository is publicly reachable, it is a valid Zane dependency.

Dependencies are declared in the project manifest (`zane.coda`) using a local alias, a URL, and an exact version tag. Source code only ever sees the alias — URLs and version strings never appear in `.zane` files. The toolchain resolves and pins the exact version at `zane add` time; subsequent builds use that pin exactly.

Libraries are distributed as **pre-compiled object files** attached to a release. The consumer's toolchain fetches and links them directly — the consumer never recompiles the library. A source-compile fallback is available for consumers who do not trust the distributor; see Platform artifacts.

---

## Manifest

Every Zane project has a `zane.coda` file at its root. Dependencies are declared in a `deps` block:

```
deps [
  # key is the local alias used in import statements; version is the exact tag name
  key  url                                    version
  math https://github.com/zane-lang/math      vers1.0.1
  http https://github.com/zane-lang/http      vers2.3.0
]
```

Each entry has three fields:

| Field | Description |
|---|---|
| key | The alias used in `import` statements and as the `$`-prefix in source code |
| url | Full repository URL — the canonical package identity |
| version | Exact release tag, e.g. `vers1.0.1`; written by the toolchain, not by hand |

The key must be a valid Zane identifier. The version field holds the exact, resolved tag name as it appears on the release — not a range, not a major-only specifier. The toolchain writes and updates this field; developers do not edit it manually.

### CLI commands

The manifest is managed through the following commands:

```sh
zane add    math https://github.com/zane-lang/math vers1.0.1
zane remove math
zane update math vers1.1.0
zane update                  # update all deps to their latest versions
```

`zane add` fetches the specified release, applies symbol substitution (see Symbol substitution at pull time), writes the entry to the `deps` block, and installs the substituted object to the global registry.

`zane update` re-resolves the specified dep (or all deps) to a new version and reinstalls.

---

## Version resolution

Zane pins **exact release tags**. When you run `zane add math https://github.com/zane-lang/math vers1.0.1`, the toolchain fetches the object file attached to the `vers1.0.1` release and writes `vers1.0.1` verbatim into `zane.coda`. All subsequent builds on all machines use that exact object file.

There is no automatic update, no floating range, no "latest compatible" resolution. The manifest is both the configuration and the lock — no separate lock file is required.

Updating a dep is always an explicit action (`zane update`). This keeps builds reproducible and avoids situations where a build produces a different binary on different machines or at different times.

---

## Symbol placeholder

When a library author compiles their library for release, the **version tag does not exist yet**. The release is created and tagged *after* the binary is built — the binary is the thing being released, so the tag cannot be known when the binary is compiled.

To break this circularity, the compiler emits a `$` prefix on every exported symbol name as a placeholder for the version tag. So a library `math` compiled without a known version produces:

```
$math$vec
$math$add
$math$dot
```

The leading `$` is the placeholder slot. The `$` between the package name and the symbol name is the Zane namespace separator (matching the grammar: `(package=IDENTIFIER '$')? name=IDENTIFIER`), and is not a placeholder.

The library author then creates the release and tags it — for example, `vers1.0.1` — and attaches the compiled object file. The binary is complete; only the tag is added after the fact.

---

## Symbol substitution at pull time

When a consumer runs `zane add math https://github.com/zane-lang/math vers1.0.1`, the toolchain:

1. Fetches the object file from the `vers1.0.1` release.
2. Reads all exported symbols matching the `$pkgname$...` pattern using `llvm-nm`.
3. Runs `llvm-objcopy --redefine-sym` to replace the leading `$` placeholder with the resolved version tag:

```
$math$vec  →  vers1.0.1math$vec
$math$add  →  vers1.0.1math$add
$math$dot  →  vers1.0.1math$dot
```

The substitution command looks like:

```
llvm-objcopy --redefine-sym $math$vec=vers1.0.1math$vec   \
             --redefine-sym $math$add=vers1.0.1math$add   \
             --redefine-sym $math$dot=vers1.0.1math$dot   \
             math.o math_vers1.0.1.o
```

The toolchain generates the full `--redefine-sym` list automatically from the symbol table; no hand-written lists are needed. The resulting substituted object is written to the global registry and reused for all future builds that reference `vers1.0.1` of this package. The original fetched object is discarded after substitution.

---

## Import and compilation

In source code, packages are imported by their local alias from the manifest:

```zane
import math
import http
```

Symbols are accessed using the `$` separator from the grammar:

```zane
math$vec(1.0, 2.0, 3.0)
http$get("https://example.com")
```

**At compile time**, the compiler looks up each alias in `zane.coda` and substitutes the versioned form everywhere that alias appears:

```
import math       →   import vers1.0.1math
math$vec(...)     →   vers1.0.1math$vec(...)
```

This substitution is performed by the compiler and is entirely invisible to the programmer. The source file never contains version strings.

### Enforcement

Every alias used in an `import` statement must exist as a key in the `deps` table. Attempting to write the versioned form directly in source code is a **compile error**:

```zane
import vers1.0.1math   # compile error — 'vers1.0.1math' is not a key in deps
```

The compiler only accepts bare alias names that match manifest keys. This rule ensures that all dependencies are declared in one place (`zane.coda`) and that the source code remains free of version details.

---

## Global registry

Packages are not stored inside the project. They are installed to a **global shared location** on the developer's machine:

```
~/.zane/registry/<package_url>/<version>/
```

Because two packages from different sources may share the same name, the full URL is used as the registry key rather than the alias. Some URL mangling is applied to produce a valid filesystem path (e.g., replacing `://` and `/` with underscores):

```
~/.zane/registry/
  https___github.com_zane-lang_math/
    vers1.0.1/
      math_vers1.0.1.o      # substituted object file, ready to link
  https___github.com_zane-lang_http/
    vers2.3.0/
      http_vers2.3.0.o
```

Consequences of the shared registry:

- The same package version is never downloaded or substituted more than once across all projects on the machine.
- Disk usage is minimised — all projects referencing `vers1.0.1` of the same URL share one object file.
- `zane build` never performs network access for deps already in the registry.

---

## Transitive dependencies

Libraries that have their own dependencies declare them in their own `zane.coda`. When a library is pulled, the toolchain reads its manifest and **automatically installs its transitive dependencies** to the global registry.

Because each library's symbols are versioned into their names (e.g., `vers2.0.0math$vec`), there are no conflicts between:

- Two top-level packages that depend on different versions of the same library
- A top-level package and one of its transitive dependencies that use different versions of a shared library

Each version occupies its own symbol namespace. The dependency graph does not need global conflict resolution — every edge independently records its exact version, and the symbol names enforce isolation at link time.

---

## Multiple versions

Because the version tag is embedded in the symbol prefix, it is completely safe to have multiple versions of the same package present in a single binary. `vers1.0.1math$vec` and `vers2.0.0math$vec` are entirely distinct symbols. The linker sees no collision. Both are present; both work correctly.

This has two useful properties:

- **No diamond problem.** If package A requires `math vers1.0.1` and package B requires `math vers2.0.0`, both versions simply coexist. No resolution step, no error, no forced upgrade.
- **More deterministic compilation.** The exact version is always explicit in the symbol name. There is no implicit global resolution that could produce a different result on a different machine or at a different time.

The tradeoff is binary size: if many different versions of the same library are pulled in transitively, all of them appear in the final binary. In practice this is uncommon and the clarity and correctness benefits outweigh the occasional size cost.

---

## Platform artifacts

Pre-compiled object files are platform-specific. The naming convention for release assets is:

```
math-vers1.0.1-x86_64-linux-gnu.o       # ELF  (Linux, x86-64)
math-vers1.0.1-aarch64-linux-gnu.o      # ELF  (Linux, ARM64)
math-vers1.0.1-x86_64-windows-msvc.obj  # COFF (Windows, x86-64)
```

The format is `{alias}-{version}-{target-triple}.{ext}`, where `.o` is used for ELF targets and `.obj` for COFF targets. The Zane toolchain uses the Zig compiler infrastructure to cross-compile objects for any supported target triple.

Library authors are expected to attach pre-compiled objects for all supported targets to each release. A typical CI script:

```
for each TARGET in supported-targets:
    zig build-obj --target TARGET src/lib.zane -o {pkg}-{version}-{TARGET}.{ext}
    attach to release
```

### Source-compile fallback

If a consumer does not trust the distributor, they can opt in to compiling the library from source instead of using the pre-built object. This is an explicit opt-in — the default is always to use the pre-built artifact for speed. Compiling from source produces the same result: after symbol substitution, the symbols are identical. The fallback is a trust and verification mechanism, not an alternative build model.

---

## Build flow

A complete build proceeds in these steps:

```
1.  Parse zane.coda — read all dep keys, URLs, and version tags.

2.  For each dep (recursively, depth-first):
    a. Check the global registry (~/.zane/registry/<url>/<version>/) for the
       substituted object.
    b. If absent:
       i.  Fetch the platform-matching release asset from the repository.
       ii. Run llvm-nm to list exported symbols matching '$pkgname$...' pattern.
       iii.Run llvm-objcopy --redefine-sym to substitute '$' prefix with version tag.
       iv. Write substituted object to the global registry.
    c. Read the dep's own zane.coda and recurse to install its transitive deps.

3.  Compile all project source files. The compiler reads zane.coda, resolves each
    import alias to its version tag, and emits versioned symbol references:
        import math    →   import vers1.0.1math
        math$vec(...)  →   vers1.0.1math$vec(...)
    Using a bare alias that is not in deps, or writing a versioned import directly
    in source, is a compile error.

4.  Link all compiled project objects against the substituted dep objects from the
    global registry.
```

---

## Summary table

| Design decision | Rationale |
|---|---|
| No central registry | No single point of failure or gatekeeping. Any public repo is a valid dep. |
| URL as package identity | Globally unique without coordination. Different packages with the same name cannot collide. |
| Exact tag pinning | Builds are fully reproducible. The manifest is both the config and the lock — no separate lock file. |
| `$` as version placeholder | The version is unknown at compile time; `$` reserves the prefix slot for later substitution. |
| Pull-time symbol substitution | The pre-built object is generic; the toolchain stamps in the exact version at install time. |
| `llvm-objcopy --redefine-sym` | Standard tool with no custom binary format. Works on both ELF and COFF targets. |
| Global shared registry | One copy of each version per machine. Disk efficient; no per-project duplication. |
| Version embedded in symbol name | Multiple versions of the same package coexist in one binary without conflict. |
| Compiler-side alias substitution | Source code never contains version strings. All version information lives in `zane.coda`. |
| Bare alias enforced in source | Prevents bypassing the manifest. All deps must be declared; accidental version coupling is impossible. |
| Transitive deps auto-installed | The consumer does not need to enumerate transitive deps; the library's manifest handles them. |
| Source-compile fallback | Trust is not mandatory. Consumers can verify by building from source without changing the workflow. |
| Zig for cross-compilation | Library authors produce platform objects with one toolchain; no separate cross-compiler setup needed. |
