# Zane Dependency Management

This document specifies how Zane identifies, fetches, versions, caches, and links external packages.

---

## 1. Overview

Zane treats a package's full source URL as its identity and pins every dependency to an exact tag plus commit hash.

- **`URL identity`.** The repository URL is canonical; local keys are only conveniences.
- **`Exact versioning`.** Each dependency records a specific tag and the commit that tag must resolve to.
- **`Prebuilt distribution`.** Libraries are distributed as source plus prebuilt object files committed to the repository.
- **`Global caching`.** Fetched and versioned artifacts are shared across projects on the machine.

---

## 2. Manifest and Resolution File

Each project records dependencies across two committed files: an intent manifest, `zane.coda`, and a resolution file, `zane-versions.coda`.

### 2.1 Manifest (`zane.coda`)

`zane.coda` records what the project wants, by package key:

```zane
zane-version v0.4.1
version-pattern v*.+.++

deps [
    key  version version-remap
    math v6.2.9  no
]
```

Top-level fields:

- **`zane-version`**: the toolchain tag used for the compiler and the `core` package; see [§14 Toolchain Version](#14-toolchain-version).
- **`version-pattern`** (required): the package author's declared ABI-compatibility window for this package's *own* versions. Every package declares one; it is established when the project is created and thereafter fixed, so a package's compatibility rule stays stable across its releases. It is information, not permission, and is consumed only when a downstream project opts into remapping; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

Each `deps` row records:

- **key**: the local camelCase package name used in source code and as the lookup key into `zane-versions.coda`
- **version**: the exact tag requested by the user
- **version-remap**: a boolean opting this dependency into compatibility-based symbol remapping; defaults to `no`

### 2.2 Resolution file (`zane-versions.coda`)

`zane-versions.coda` records how each key resolves to a concrete source and commit:

```zane
[
    key  url                                commit
    zane https://github.com/zane-lang/zane  9f1c0aa
    math https://github.com/zane-lang/math  a3f8c2d
]
```

Each row records:

- **key**: matches a `deps` key in `zane.coda`. The reserved key `zane` holds the resolution of the `zane-version` toolchain tag and **MUST NOT** be used as an ordinary dependency key.
- **url**: the canonical package identity.
- **commit**: the exact commit that the recorded tag must resolve to.

The repository URL is canonical identity; the key is only a local convenience for naming the package in source and joining the two files. Both files are committed. Users update them through CLI commands rather than by manual editing.

### 2.3 Files are recorded and updated by commands
`zane add` resolves the requested tag to its current commit hash, writes the key, tag, and remap flag into `zane.coda`, and writes the key, url, and commit into `zane-versions.coda`. The user does not type the commit hash manually in the normal workflow.

`zane update key version` replaces the recorded tag in `zane.coda` and the recorded commit in `zane-versions.coda` for that key, keeping the two files in sync. A whole-project update re-resolves each dependency and refreshes both files.

If a tag has moved and the user intentionally wants to trust the new commit, the update flow requires an explicit override flag rather than silently refreshing the hash, for example:

```sh
zane update math v6.2.9 --accept-tag-move
```

---

## 3. Repository Layout

Library repositories include prebuilt objects in the git tree:

```zane
math/
  src/
  build/
  zane.coda
```

Source files live under `src/`. Prebuilt object files for all supported target triples live under `build/`. The toolchain selects the correct prebuilt artifact for the current target triple from `build/`.

---

## 4. Tag and Commit Verification

When the toolchain fetches a dependency, it resolves the recorded tag to a current commit hash and compares it to the commit recorded in `zane-versions.coda`.

- If the hashes match, fetch proceeds.
- If the hashes differ, the fetch **MUST** abort with a security error.

This detects moved tags and repository tampering.

---

## 5. Fetching

Fetching is a normal git fetch/clone against the package URL. Because binaries live in the repository tree under `build/`, Zane does not depend on host-specific release-asset APIs.

If the required target artifact is missing from `build/`, the fetch fails for that target.

---

## 6. Symbol Versioning

### 6.1 Placeholder-prefix rewriting
Libraries are compiled with their own exported symbols prefixed by the placeholder marker `!`. During `zane add`, the toolchain rewrites those symbols — replacing the `!` prefix with the resolved version tag followed by an `@` separator — and places the rewritten binaries into `build/`.

Conceptually:

```zane
!math$vec  →  v1.0.1@math$vec
```

The `@` separates the version tag from the package key so the version boundary is unambiguous and two different packages can never collide on a shared prefix. Because path-safety validation (§7) forbids `@` in version tags, the first `@` always delimits the version from the key during the remap rewrite.

The `!` prefix is reserved for this toolchain placeholder role and is not a valid user-defined identifier prefix. The original `!`-prefixed object files are those committed to the repository's own `build/` directory; the rewritten, version-stamped object files are written to the cache's top-level `build/` directory. Only the fetched library's own placeholder-prefixed exports are rewritten; already-versioned transitive references remain unchanged.

### 6.2 Why rewrite symbols
Versioned symbol names allow multiple versions of the same package to coexist in one program without collisions.

### 6.3 Transitive dependencies keep their resolved versions
When a library already depends on another versioned library, the referenced transitive symbols are left as-is. Only the fetched library's own placeholder-prefixed exports are rewritten.

### 6.4 Optional compatibility-based remapping
When a consumer opts in, version-prefixed symbols may additionally be remapped at link time to collapse interchangeable versions of a package onto a single copy. This is layered on the same rewrite step; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

---

## 7. Global Package Cache

Fetched packages are stored in a global cache shared across projects:

```zane
~/.zane/packages/<mangled_url>/<mangled_version>/
  src/
  build/
```

The URL and version are mangled into safe path components using Go-style path mangling: each `/` in the URL produces a new subdirectory level, so `github.com/zane-lang/math` becomes `github.com/zane-lang/math` as nested directories. If the URL or version tag contains any character that is not safe to use directly as a path component — such as `:`, `@`, `?`, `#`, or any other character that would be illegal or ambiguous on the host filesystem — `zane add` **MUST** fail immediately with an error rather than attempting to mangle or escape the offending character.

The `src/` subdirectory holds the full cloned repository, including the repository's own `src/` and `build/` directories; the original `!`-prefixed object files committed by the library author are therefore found at `src/build/`. The top-level `build/` subdirectory holds the rewritten, version-stamped object files produced during `zane add`. Re-adding the same package version in another project reuses the existing cached `build/` artifact rather than downloading and rewriting it again.

A fully expanded cache path for the `math` example therefore looks like:

```zane
~/.zane/packages/github.com/zane-lang/math/v1.0.1/
  src/
  build/
```

---

## 8. Import Syntax

Source code imports by manifest key:

```zane
import math
```

And uses package members through that key:

```zane
math$vec(...)
```

Source code never writes version-prefixed package names directly. The compiler resolves keys through `zane.coda` and `zane-versions.coda`.

---

## 9. Transitive Dependencies

When a package is fetched, the toolchain recursively reads its `zane.coda` and installs all transitive dependencies needed by that package version before treating the package as ready to link.

---

## 10. Package Dependency Graph

The package dependency graph **MUST** be a directed acyclic graph (DAG). Cyclic imports across package boundaries are not allowed.

If package `A` imports package `B`, then package `B` **MUST NOT** import package `A`, either directly or transitively through any chain of intermediate packages. A package therefore **MUST NOT** import itself, directly or indirectly.

The compiler **MUST** detect and reject cyclic package dependencies at build time with an error message that identifies the cycle.

### 10.1 Single-package mutual references
Within a single package, source files may freely reference each other's declarations. A package is compiled as one unit, so mutual references among declarations in the same package are legal and do not constitute a cycle.

The acyclicity requirement applies only to the package-level dependency graph, not to intra-package references.

---

## 11. Multiple-Version Coexistence

Two packages may depend on different versions of the same upstream library. Because symbol names are version-prefixed at fetch time, both versions may coexist in one final link as long as all references are internally consistent.

This side-by-side coexistence is the default. A consumer may opt into collapsing interchangeable versions onto a single copy via compatibility-based remapping; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

---

## 12. Platform Artifacts

Packages may ship multiple prebuilt object files for different target triples under `build/`. A dependency is usable on a target only if the repository contains the matching prebuilt artifact for that target.

### 12.1 Source compilation is explicit opt-in
The normal workflow consumes the repository's checked-in prebuilt artifact from `src/build/`. A user who does not trust the shipped object file may opt into local compilation from the verified source checkout under `src/src/` instead.

```sh
zane add math https://github.com/zane-lang/math v1.0.1 --from-source
```

This is an explicit trust/debugging escape hatch, not the default package-distribution model.

---

## 13. Build Flow

At a high level, dependency resolution proceeds in this order:

1. read local `zane.coda` and `zane-versions.coda`
2. resolve each tag to its current commit hash
3. verify commit hashes against `zane-versions.coda`
4. validate that the URL and version tag contain only path-safe characters; abort with an error if not
5. read transitive manifests and reject the dependency if the package graph contains a cycle, with an error that identifies the cycle
6. clone the repository into `~/.zane/packages/<mangled_url>/<mangled_version>/src/`
7. rewrite the `!`-prefixed exports found in `src/build/` with the resolved version tag and write the results to `~/.zane/packages/<mangled_url>/<mangled_version>/build/`
8. for any package whose `version-remap` the consumer enabled, group the required versions by declared `version-pattern`, collapse interchangeable versions onto the chosen version, and remap displaced references; keep non-interchangeable versions side by side, warning on divergent patterns (see [§15](#15-compatibility-patterns-and-remapping))
9. link the locally compiled program against the cached artifacts in `build/`

---

## 14. Toolchain Version

The `zane-version` field in `zane.coda` pins the toolchain tag used to build the project. It selects both the compiler and the `core` package release; the reserved `zane` key in `zane-versions.coda` records the commit that tag must resolve to.

- The compiler and `core` are released together under one tag, so a project always builds with a known toolchain. This frees the toolchain to evolve without preserving backward compatibility across versions: each project states the version it builds with.
- The standard library beyond `core` is **not** special. `std` is a separate package fetched, versioned, and remapped like any other dependency, with its own `deps` row in `zane.coda` and entry in `zane-versions.coda`.
- The reserved `zane` key is subject to the same tag/commit verification as every other entry (§4): a moved toolchain tag is detected, not silently trusted.

---

## 15. Compatibility Patterns and Remapping

By default, when two parts of the dependency graph require different versions of the same package, both versions are linked side by side using version-prefixed symbols (§6, §11). Compatibility-based remapping is an **opt-in** optimization that collapses such versions onto a single copy when doing so is declared safe, eliminating the duplicate.

### 15.1 Roles: author declares, consumer decides
- **Author (`version-pattern`).** A package author publishes a `version-pattern` in the package's own `zane.coda`. It is **information, not permission**: it declares the range of the package's own versions that are interchangeable at the ABI level. It never forces remapping on or off.
- **Consumer (`version-remap`).** The top-level project decides, per dependency, whether to remap, via the boolean `version-remap` column in its `deps` block. This is the **only** place the remap decision is made. `version-remap` flags in transitively-fetched libraries' manifests are ignored; an intermediate library cannot force a package it depends on to be remapped or kept separate.

Remapping of a package occurs only when the consumer enables it **and** the published patterns make it ABI-safe. Otherwise the versions coexist unchanged.

### 15.2 Pattern syntax
A `version-pattern` mirrors the shape of the package's version tags, replacing each numeric component with a marker:

- `*` — **fixed boundary.** This component must match exactly for two versions to be interchangeable. It carries no priority and does not participate in selection. (Typically the major component.)
- `+` / `-` — **directional and priority-bearing.** `+` means the component is upward-substitutable (a higher value is a valid replacement); `-` means downward-substitutable. The marker is repeated to encode priority.

**Priority is markdown-style: fewer repeats means higher priority.** `+` outranks `++` outranks `+++`. Priority is always explicit — there is no positional default — so a pattern is fully self-describing in isolation.

Example: `v*.+.++` reads as "same major; among interchangeable versions prefer the highest minor first (`+`, top priority), breaking ties by highest patch (`++`)."

#### 15.2.1 Validation rules
- Every `+`/`-` component **MUST** carry an explicit priority via its repetition count.
- No two `+`/`-` components may share a priority level. A pattern with a duplicate level is **rejected at parse time**; this strict total order is what makes selection deterministic.
- `*` components carry no priority and are excluded from the ordering.

### 15.3 Selection ("best of both")
With remapping enabled for a package, the toolchain considers the set of versions required across the graph and groups them by their declared `version-pattern` string:

1. Within a group sharing an identical pattern, one version may **substitute** for another when every `*` component is equal and the directional components agree under a **hierarchical** comparison: the `+`/`-` components are examined in priority order (highest priority — fewest repeats — first), and the first component at which the two versions differ must differ in that component's declared direction (greater for `+`, smaller for `-`). Once a higher-priority component satisfies its direction, lower-priority components are unconstrained — so a minor bump that resets the patch to `0` still substitutes. Versions equal in every component substitute trivially.
2. The **chosen** version is the optimum under this priority order — for `+` the greatest value at the highest-priority component, for `-` the least, with lower-priority components breaking ties — provided every other version in the group may substitute onto it. The strict total order guarantees a single deterministic winner, so the link is reproducible.
3. References to the displaced versions are remapped onto the chosen version and the displaced copies are dropped from the link.

### 15.4 When versions are not interchangeable
- **Same pattern, out of window** (for example, a `*` major component differs): the versions are kept side by side, as in the default model. This is expected and produces **no warning**.
- **Different patterns**: versions of the same package that declare *different* `version-pattern` strings are never remapped onto each other. They are kept side by side and the toolchain emits an **informational warning** (not a security error) noting that divergent patterns prevented full deduplication. Other versions that do share a pattern still collapse normally.

### 15.5 Safety: this is an ABI assertion on prebuilt objects
Because libraries ship prebuilt object files (§3, §6), remapping rewrites a caller's symbol references to point at a different version's compiled objects. The author's `version-pattern` therefore asserts **ABI** compatibility across the window — identical signatures, type layouts, and calling conventions — which is a stronger promise than source/API compatibility. A wrong assertion produces silent undefined behavior at link time, with no recompilation to catch it. For this reason remapping is opt-in per consumer, defaults to `no`, and always degrades to safe coexistence when a single common version cannot be shown interchangeable.

### 15.6 Mechanism reuses pull-time rewriting
Remapping is a link-time pass layered on the symbol rewriting of §6.1. Exact pins are untouched: every required version remains recorded in `zane.coda` / `zane-versions.coda` and fetched. The pass only chooses which cached objects to link and rewrites the displaced references — conceptually `v6.2.9@math$vec → v6.3.4@math$vec` — onto the chosen version.

---

## 16. Design Rationale

| Decision | Rationale |
|---|---|
| URL as package identity | Avoids global registry naming conflicts and matches how code is actually fetched. |
| Exact tag plus commit hash | Keeps builds reproducible and makes moved tags detectable. |
| Prebuilt objects in the repository under `build/` | Keeps fetching host-agnostic and lets source and artifacts share one immutable commit identity. |
| Pull-time symbol versioning | Enables multiple versions to coexist without requiring source-level version names. |
| `src/` and `build/` separation in the cache | Makes the distinction between the original `!`-prefixed objects from `src/build/` and the rewritten version-stamped objects explicit and auditable. Reuse across projects reads from `build/` without repeating the rewrite step. |
| Go-style URL-to-path mangling with hard failure on illegal characters | Keeps cache paths human-readable and debuggable while preventing ambiguous or unsafe paths from ever being created. |
| Global package cache | Avoids repeated downloads and rewrite work across projects. |
| Key-based imports | Keeps source code readable while the manifest and resolution file remain the single source of truth for version resolution. |
| Package dependency graph is a DAG | Simplifies compilation order, prevents initialization deadlocks, and makes dependency reasoning straightforward. Declarations within one package may still reference each other freely because the package is compiled as one unit. |
| Manifest/resolution split (`zane.coda` + `zane-versions.coda`) | Keeps human-stated intent (key, version, remap) separate from resolved addressing (url, commit), so intent reviews stay small while the resolved commits act as a verifiable lock. |
| `zane-version` pins compiler and `core` together | Each project builds with a known toolchain, freeing the toolchain to evolve without cross-version backward-compatibility guarantees. |
| `std` is an ordinary package | Avoids privileged standard-library coupling; only `core` rides the toolchain tag. |
| Author-declared `version-pattern`, consumer-decided `version-remap` | Separates the compatibility *facts* (only the author knows them) from the *decision* to trust them (only the consumer bears the ABI risk). |
| Markdown-style explicit priority (fewer repeats = higher) | Makes every pattern self-describing with no positional convention to assume, supporting arbitrary and custom version schemes. |
| Remap as opt-in link-time dedup over exact pins | Removes duplicate near-identical versions from the binary without a version solver or any loss of reproducibility; safe coexistence remains the default. |
| Warning on divergent patterns | Surfaces a likely-unintended compatibility gap as information, without blocking the build or overstating it as a security issue. |
