# Zane Dependency Management

This document specifies how Zane resolves, fetches, versions, and links external packages. It covers the manifest format, the CLI commands, symbol versioning at pull time, the global package cache, and the end-to-end build flow.

---

## 1. Overview

Zane has no central package registry. A package is identified by its full source URL — typically a GitHub or Codeberg repository URL. Two packages from different hosts can share the same short name without conflict because the URL, not the name, is the canonical identity.

Libraries are distributed as **pre-compiled object files committed directly into the repository**. The Zane toolchain clones the repository at the specified tag and uses the object files from the repository tree. The consumer never recompiles the library unless they explicitly opt in.

Versions are **exact tags pointing to exact commits**. There is no implicit resolution, no semver range matching, and no global lockfile negotiation across packages. The exact tag is chosen by the user on the command line and written into the project manifest alongside the commit hash it must resolve to. What is in the manifest is what gets linked.

> **See also:** [`rationale.md`](rationale.md) §5 for the reasoning behind each design decision.

---

## 2. Manifest

Every Zane project has a `zane.coda` file at its root. Dependencies are declared in a `deps` block:

```
deps [
    # key is the local alias used in import statements
    # url is the full package URL
    # version is the exact tag
    # commit is the exact commit hash the tag must point to
key url version commit
    math https://github.com/zane-lang/math vers1.0.1 a3f8c2d
    http https://github.com/zane-lang/http vers2.3.0 b91e4f7
]
```

Each entry has four fields: a short **key** (the alias used in source code), the full **URL**, the exact **version tag**, and the **commit hash** that the tag must resolve to. The user never edits `zane.coda` directly. All changes go through CLI commands:

```sh
zane add math https://github.com/zane-lang/math vers1.0.1
zane remove math
zane update math vers1.1.0
zane update           # re-fetches all deps and resolves each to its latest tag
```

When `zane add` fetches a package it records both the tag and the commit hash the tag currently points to. Both are written into `zane.coda`. The commit hash is the trust anchor — the tag is a human-readable label, but the commit hash is what the toolchain actually enforces.

`zane remove` removes the entry and any packages no longer referenced by the project.

`zane update` with a version argument pins the alias to the new tag, resolves it to its commit hash, and updates both fields in `zane.coda`. Without arguments, it queries each repository's tags and pins each to the latest, updating both tag and commit hash fields.

### 2.1 Commit hash commands

`zane add` always records the commit hash automatically. The user never supplies it manually:

```sh
zane add math https://github.com/zane-lang/math vers1.0.1
# fetches, verifies tag resolves to a commit, writes both tag and commit into zane.coda
```

`zane update math vers1.1.0` pins the alias to the new tag, resolves it to its current commit hash, and updates both the tag and commit hash fields in `zane.coda`. The old commit hash is discarded.

`zane update` (no arguments) re-resolves each tag to its latest and updates both the tag and commit hash fields in `zane.coda`. This is the only normal workflow in which commit hashes change in the manifest.

`zane update math vers1.0.1 --force-recommit` is the escape hatch for the tag mismatch case. It re-resolves the existing tag to its current commit hash and overwrites the recorded commit hash with the new value. It requires an explicit flag and prints a warning reminding the user to audit the new commit before trusting it:

```
WARNING: Force-recommitting math to a new commit.

  Tag     : vers1.0.1
  Old hash: a3f8c2d
  New hash: f00ba12

  You are accepting a tag that points to a different commit than before.
  Verify that the new commit is trustworthy before building.

  The manifest will be updated if you proceed.
```

---

## 3. Repository Layout

Library authors commit their pre-built object files directly into the repository alongside source. A typical library repository looks like:

```
math/
  src/
    vec.zane
    add.zane
  prebuilt/
    math-x86_64-linux.o
    math-aarch64-macos.o
    math-x86_64-windows.o
  zane.coda
```

The pre-built object files are part of the git tree. When a tag is created, the tag points to a commit, and that commit includes both the source and the binaries. **The binaries are therefore as immutable as the commit itself** — they cannot be swapped out without creating a new commit and moving the tag, which is detectable by the manifest's commit hash verification.

Library authors are strongly encouraged to enable **tag protection** on their forge (GitHub, Codeberg, etc.) to prevent tags from being force-pushed to a different commit. With tag protection enabled and binaries in the repo tree, a tagged version is fully immutable. Even without forge-level tag protection, the manifest's commit hash check provides a cryptographic guard.

---

## 4. Tag and Commit Verification

Every time a dependency is fetched on a machine that does not already have it cached, the toolchain resolves the tag to its current commit hash and compares it against the hash recorded in `zane.coda`:

```
recorded in zane.coda:  math  vers1.0.1  a3f8c2d
resolved from remote:   vers1.0.1  →  a3f8c2d   ✅ match, fetch proceeds
```

```
recorded in zane.coda:  math  vers1.0.1  a3f8c2d
resolved from remote:   vers1.0.1  →  f00ba12   ❌ mismatch, fetch aborted
```

If the hashes do not match, the toolchain **aborts the fetch for that library** and prints a prominent error:

```
SECURITY ERROR: tag mismatch for math (https://github.com/zane-lang/math)

  Tag     : vers1.0.1
  Expected: a3f8c2d  (recorded in zane.coda)
  Got     : f00ba12  (current remote)

  The tag has been moved to a different commit since this dependency was added.
  This may indicate a compromised repository or an intentional release change.

  To accept the new commit and update the manifest:
      zane update math vers1.0.1 --force-recommit

  Do not do this unless you trust the new commit and understand why the tag moved.
```

The build does not proceed for any target that depends on the affected library. Other libraries whose tags resolve correctly are fetched and cached as normal.

---

## 5. Fetching

Because binaries live in the repository itself, fetching is a straightforward shallow clone at the verified commit:

```sh
git clone --depth 1 --branch vers1.0.1 https://github.com/zane-lang/math
# After clone, verify that HEAD is at a3f8c2d
git rev-parse HEAD  # must equal a3f8c2d
```

This works on **any git host** without forge-specific release asset APIs. No special integration with GitHub releases, Codeberg attachments, or any other forge-specific mechanism is required. Any host that speaks git works out of the box.

The toolchain selects the correct object file from the `prebuilt/` directory based on the current target triple (e.g. `x86_64-linux`, `aarch64-macos`, `x86_64-windows`). If the target is not available in `prebuilt/`, the fetch aborts with an error indicating that the library does not support the target.

---

## 6. Symbol Versioning

When a consumer runs `zane add`, the toolchain fetches the pre-compiled object file and rewrites every symbol that carries the `!` placeholder prefix — replacing `!` with the version tag:

```
!math$vec  →  vers1.0.1math$vec
!math$add  →  vers1.0.1math$add
```

This rewrite is performed using `llvm-objcopy --redefine-sym`. The rewritten object file is written to the global packages directory under the package URL and version (see §7). From that point on, the version tag is permanently part of the symbol name embedded in the object file.

### 6.1 The `!` placeholder convention

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

## 7. Global Package Cache

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

## 8. Import Syntax

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

## 9. Transitive Dependencies

Libraries declare their own dependencies in their own `zane.coda`. When a package is pulled, the toolchain reads its manifest and **recursively installs all transitive dependencies** into the global packages directory before the package itself is considered ready.

When a library is compiled, the compiler emits its own symbols with the `!` placeholder prefix (e.g. `!math$vec`). References to other packages it depends on are emitted as fully versioned symbols, because those packages were already pulled and versioned in the author's build environment. By the time a consumer pulls the library, the `!`-prefixed own symbols are rewritten with the version tag; the already-versioned transitive references are left unchanged.

Because versioned symbols are distinct strings, two packages that depend on **different versions of the same library** coexist cleanly. If `plot` requires `vers2.0.0math` and `http` requires `vers1.5.0math`, both are installed, and their symbols (`vers2.0.0math$*` and `vers1.5.0math$*`) are entirely distinct. No conflict, no resolution algorithm, no diamond problem.

---

## 10. Multiple Version Coexistence

Because version tags are embedded in symbol names, multiple versions of the same library can coexist in a single binary without any conflict:

```
vers1.0.1math$vec    // from one dep's transitive chain
vers2.0.0math$vec    // from another dep's transitive chain
```

These are distinct symbols. The linker sees no ambiguity. There is no implicit global resolution that could vary between machines or build environments.

This also makes builds more deterministic: the exact version is always explicit in every symbol reference, and the resolved version is pinned in `zane.coda`. Two developers with the same manifest always link the same symbols.

Tradeoff: if a project transitively pulls in several major versions of a large library, binary size grows proportionally. There is no deduplication across versions — each version is a separate set of symbols. This is an intentional tradeoff: correctness and determinism are prioritised over binary size optimisation.

---

## 11. Platform Artifacts

Pre-compiled object files are platform-specific. A release commit includes one object file per supported target triple in the `prebuilt/` directory:

```
prebuilt/
  math-x86_64-linux.o
  math-aarch64-macos.o
  math-x86_64-windows.o
```

The toolchain selects the correct artifact based on the current target at pull time.

Because the Zane compiler uses the [Zig](https://ziglang.org) toolchain for cross-compilation, library authors can produce all platform artifacts from a single machine without CI:

```sh
zig build-lib -target x86_64-linux   math.zane -femit-bin=prebuilt/math-x86_64-linux.o
zig build-lib -target aarch64-macos  math.zane -femit-bin=prebuilt/math-aarch64-macos.o
zig build-lib -target x86_64-windows math.zane -femit-bin=prebuilt/math-x86_64-windows.o
git add prebuilt/
git commit -m "release vers1.0.1"
git tag vers1.0.1
git push && git push --tags
```

No CI infrastructure is required. The author builds locally, commits the artifacts, and tags the commit. The tag now immutably captures both source and binaries in a single git object.

### 11.1 Source-compile opt-in

By default, the toolchain uses the pre-compiled object file from the repository. A consumer who does not trust the pre-built artifacts can opt in to **compiling the package from source** instead:

```sh
zane add math https://github.com/zane-lang/math vers1.0.1 --from-source
```

This clones the repository at the specified tag (with commit hash verification), compiles it locally from source, and installs the result to the packages directory in place of the pre-built artifact. The output is functionally identical to the pre-built object after symbol prefixing. The `--from-source` flag is a trust/verification escape hatch, not a normal workflow.

---

## 12. Build Flow

The end-to-end flow from library authorship to consumer compilation:

```
1. Library author writes and compiles the library locally.
   Compiler emits !math$vec, !math$add, ... using ! as a version placeholder prefix.
   Author cross-compiles for all supported targets using the Zig toolchain.
   Resulting object files are committed to prebuilt/ in the repository.

2. Author creates a version tag (e.g. vers1.0.1) pointing to the commit.
   The tag captures both source and prebuilt binaries as a single git object.
   Tag protection on the forge is recommended to prevent the tag from being moved.

3. Consumer runs: zane add math https://github.com/zane-lang/math vers1.0.1
   Toolchain resolves the tag on the remote to its commit hash.
   Toolchain records both tag and commit hash in zane.coda.
   Toolchain performs: git clone --depth 1 --branch vers1.0.1 <url>
   Toolchain verifies the cloned HEAD matches the recorded commit hash.
   Toolchain selects the correct object file from prebuilt/ for the current target.
   Toolchain runs llvm-objcopy to rewrite !-prefixed symbols with the version tag.
   Prefixed object file is written to the global packages directory.
   Manifest entry is appended to zane.coda.
   Transitive deps from the library's zane.coda are fetched recursively.
   Each transitive dep undergoes the same tag/commit verification before fetching.

4. Team member clones the project and runs: zane build
   For each dependency not already in the global packages directory:
       Toolchain resolves the tag on the remote.
       Toolchain compares the resolved commit hash against the hash in zane.coda.
       If they match, fetch proceeds normally as in step 3.
       If they do not match, fetch is aborted with a SECURITY ERROR and the build stops.

5. Consumer writes source code:
       import math
       v = math$vec(1.0, 2.0)

6. Consumer runs: zane build
   Compiler reads zane.coda, resolves math → vers1.0.1.
   Compiler substitutes: math$vec → vers1.0.1math$vec throughout the IR.
   Linker links against ~/.zane/packages/https/github.com/zane-lang/math/vers1.0.1/math.o.
   Binary is produced.
```
