# Zane Dependency Management

This document specifies how Zane identifies, fetches, versions, caches, and links external packages.

---

## 1. Overview

Zane treats a project's full source URL as its identity and pins every dependency to an exact tag plus commit hash. A dependency is a whole project: the library packages it publishes ([`packages.md`](packages.md) §4.3) share its URL, its version, and its pin, and this document calls the fetched project a **package** where no confusion with its library packages can arise.

- **`URL identity`.** The repository URL is canonical; local keys are only conveniences.
- **`Exact versioning`.** Each dependency records a specific tag and the commit that tag must resolve to.
- **`Prebuilt distribution`.** Libraries keep source in Git and publish prebuilt object archives as GitHub Release assets. A committed artifact manifest pins their download URLs and SHA-256 hashes.
- **`Global caching`.** Fetched and versioned artifacts are shared across projects on the machine.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#url-identity-and-the-two-file-manifest) — "URL identity and the two-file manifest" weighs URL identity against a central registry and records what the registry road would have bought.

---

## 2. Manifest and Resolution File

Each project records dependencies across two committed files: an intent manifest, `zane.coda`, and a lock file, `zane-lock.coda`.

### 2.1 Manifest (`zane.coda`)

`zane.coda` records what the project wants, by dependency key:

```zane
zane-version v0.4.1
version-pattern v*.+.++

deps [
    key      version  from
    core     v1.4.0   release
    math     v6.2.9   source
    geometry v0.3.0   ../geometry
]

remaps [
    https://github.com/zane-lang/math
]
```

Top-level fields:

- **`zane-version`**: the toolchain tag used for the compiler; see [§14 Toolchain Version](#14-toolchain-version).
- **`version-pattern`** (required): the package author's declared ABI-compatibility window for this package's *own* versions. Every package declares one; it is established when the project is created and thereafter fixed, so a package's compatibility rule stays stable across its releases. A manifest that omits `version-pattern` is malformed: the toolchain **MUST** reject it with an error rather than treating the package as unversioned or remappable. It is information, not permission, and is consumed only when a downstream project opts into remapping; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

Each `deps` row records:

- **key**: a camelCase label for the dependency, which commands name it by and which joins the row to `zane-lock.coda`; source code never writes it, since an import names a library package (§8)
- **version**: the exact tag requested by the user
- **from**: where the dependency's code comes from — `release` for the verified prebuilt archive (§5), `source` for local compilation of the verified checkout (§12.1), or a local project path beginning with `./`, `../`, or `/` (§12.2)

The `from` column takes effect only in the manifest of the project being built. In a transitively fetched manifest it is ignored, and every dependency of that package is fetched as `release`.

The optional top-level **`test-deps`** block lists the dependencies whose packages only the project's test packages import ([`packages.md`](packages.md) §7.3). It has the same columns as `deps`, and its rows follow the same rules:

```zane
zane-version v0.4.1
version-pattern v*.+.++

deps [
    key  version  from
    core v1.4.0   release
]

test-deps [
    key      version  from
    mocks    v0.2.0   release
    zaneTest v1.1.0   release
]
```

A `test-deps` block takes effect only in a test build of the project whose manifest holds it. In a transitively fetched manifest it is ignored, so a project's test dependencies never enter its consumers' dependency graphs.

No key appears in both `deps` and `test-deps`.

The optional top-level **`remaps`** block is a bare list of the canonical package **URLs** the consumer opts into compatibility-based symbol remapping (a single-column coda array, so it has no header row). It lists URLs rather than keys because a key is only a local nickname scoped to one project, whereas remapping may target a package that appears **only transitively** and therefore has no key in this project's `deps`; the URL is the canonical, globally unambiguous identity, so any package in the resolved graph can be named whether or not it is a direct dependency. Listing a URL requires no version pin, since the versions come from the resolved graph. A package whose URL is absent from `remaps` is never remapped and its versions coexist side by side (the default). A URL listed in `remaps` that matches no package in the resolved dependency graph is a likely stale entry or typo; the toolchain emits an informational warning during dependency resolution (not an error) so the manifest can be kept clean. `remaps` is the **only** place the remap decision is made; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

### 2.2 Lock file (`zane-lock.coda`)

`zane-lock.coda` records how each key resolves to a concrete source and commit:

```zane
resolutions [
    key  url                                commit
    zane https://github.com/zane-lang/compiler  9f1c0aa
    core https://github.com/zane-lang/core  4b7e91c
    math https://github.com/zane-lang/math  a3f8c2d
]
```

Each row records:

- **key**: matches a `deps` or `test-deps` key in `zane.coda`. The reserved key `zane` holds the resolution of the `zane-version` toolchain tag and **MUST NOT** be used as an ordinary dependency key.
- **url**: the canonical package identity.
- **commit**: the exact commit that the recorded tag must resolve to.

The repository URL is the canonical identity; the key is only a local label for naming the dependency in commands and joining the two files. Both files are committed. Users update them through CLI commands rather than by manual editing.

The two files **MUST** stay in sync: every `deps` and `test-deps` key in `zane.coda`, plus the reserved `zane` key, **MUST** have exactly one matching `resolutions` row in `zane-lock.coda`, and every `resolutions` row **MUST** correspond to such a key. The toolchain validates this when reading the files (build flow step 1) and **MUST** abort with an error on any missing, extra, or mismatched key rather than guessing the user's intent.

A dependency's `from` value leaves its pinned tag and its `resolutions` row unchanged. A `source` or path dependency keeps both, so setting `from` back to `release` returns to the pinned version.

This pair records a project's **direct** dependencies only; it is not a flattened lock of the whole graph. Transitive dependencies never appear in a project's own `zane-lock.coda` (which is exactly why the [`remaps` block names URLs rather than keys](#21-manifest-zanecoda) — a transitive-only package has no row here to key off). Reproducibility of the *entire* graph still holds, because every dependency commits its **own** `zane.coda` / `zane-lock.coda`, each pinning its own direct dependencies to exact commits, and the resolver walks those committed files recursively (build flow step 5). Since every edge is pinned to an immutable commit, the transitive closure of these per-package lock files reproduces the full graph exactly, with no need to flatten transitive entries into the top-level file. The strict sync rule above therefore governs each package's two files in isolation, at every level of the graph.

### 2.3 Files are recorded and updated by commands

`zane add` resolves the requested tag to its current commit hash, writes the key and tag into the `deps` block of `zane.coda`, and writes the key, url, and commit into `zane-lock.coda`. The user does not type the commit hash manually in the normal workflow. `zane add` records `release` in the `from` column unless asked for `source` (§12.1), and writes the row into `test-deps` instead of `deps` when asked with `--test`. `zane dev key path` sets a dependency's `from` to a local path (§12.2), and `zane dev off key` sets it back to `release`. Remap opt-in is recorded separately in the `remaps` block.

`zane update key version` replaces the recorded tag in `zane.coda` and the recorded commit in `zane-lock.coda` for that key, keeping the two files in sync. A whole-project update re-resolves each dependency and refreshes both files.

If a tag has moved and the user intentionally wants to trust the new commit, the update flow requires an explicit override flag rather than silently refreshing the hash, for example:

```sh
zane update math v6.2.9 --accept-tag-move
```

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#url-identity-and-the-two-file-manifest) — "URL identity and the two-file manifest" explains why intent and lock are split, and why drift is contained by a hard sync check rather than by merging the files.
> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#where-a-dependencys-code-comes-from) — "Where a dependency's code comes from" explains why `from` is a manifest column rather than a command flag or a lock-file entry, and why the lock file is named `zane-lock.coda`.
> **Story:** [`stories/packages.md`](../stories/packages.md#a-librarys-tests-live-in-test-and-import-it-as-a-consumer) — "A library's tests live in `test/` and import it as a consumer" explains why test dependencies are a block of the root manifest.

---

## 3. Repository Layout

Project repositories contain source and committed metadata:

```zane
geometry/
  lib/
  bin/
  test/
  zane.coda
  zane-lock.coda
  zane-artifacts.coda
```

Library packages live in `lib/`, program packages in `bin/`, and test packages in `test/` ([`packages.md`](packages.md) §2.1). A release archive holds the objects of the `lib/` packages alone. Prebuilt object files are published outside the Git tree as release archives (§3.1). The source repository URL remains the package identity; the artifact download URL is only a location.

### 3.1 Artifact manifest (`zane-artifacts.coda`)

A project with library packages commits `zane-artifacts.coda` at its repository root. Its `artifacts` table describes the prebuilt objects of its library packages, with one row per supported target triple:

```zane
artifacts [
    target                   url                                                                                                      sha256
    x86_64-unknown-linux-gnu  https://github.com/zane-lang/math/releases/download/v1.0.1/math-x86_64-unknown-linux-gnu.tar.gz             4e07408562bedb8b60ce05c1decfe3ad16b72230967de01f640b7e4729b49fce2
    aarch64-apple-darwin      https://github.com/zane-lang/math/releases/download/v1.0.1/math-aarch64-apple-darwin.tar.gz                 4b227777d4dd1fc61c6f884f48641d02b4d121d3fd328cb08b5531fcacdabf8a
]
```

The hashes above illustrate the field format, not actual published archives. Each row records:

- **`target`**: the exact target triple used to select the artifact.
- **`url`**: an absolute HTTPS download URL chosen by the package author. GitHub Releases is the initial publishing host. The fetcher accepts an ordinary HTTPS file URL and does not infer a host, asset name, or release from the source repository URL.
- **`sha256`**: exactly 64 lowercase hexadecimal digits containing the SHA-256 digest of the complete compressed archive bytes.

The toolchain **MUST** reject malformed rows, non-HTTPS URLs, invalid hashes, and duplicate target triples. It reads this file only from the verified source commit (§4), including for transitive packages. Consumers do not duplicate the artifact hashes in their own lock files: their pinned commits already pin each dependency's artifact manifest.

Each asset is a gzip-compressed tar archive containing a `build/` directory with the original object files of every library package of the project, subpackages and `_`-prefixed packages included, and their placeholder-prefixed exports (§6.1). It contains no vendored transitive dependency objects; those dependencies are fetched through their own manifests (§9). The archive contains only directories and regular files under the `build/` directory. Extraction **MUST** reject absolute paths, `..` path components, links, and any entry that would escape the artifact's extraction directory.

### 3.2 Publishing a release

The author builds the objects for each supported target from the release's source and dependency pins, packages them, and computes each archive's SHA-256. The author then commits the URLs and hashes in `zane-artifacts.coda`, tags that commit, and uploads the exact archives at the recorded GitHub Release URLs. The release is usable only after its assets are available. Archives contain the objects, not the artifact manifest, so recording their hashes creates no circular hash dependency.

A release **MUST NOT** be published while any `deps` row of the package's manifest has a path `from` (§12.2), because no other machine can fetch the code such a row names. A `test-deps` row may keep a path `from`, since no consumer reads it.

Changing an archive requires publishing a new package version with a new committed hash. Replacing an asset at an existing URL with different bytes does not update consumers' pins; verification fails (§5).

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#release-assets-with-committed-hashes) — "Release assets with committed hashes".

---

## 4. Tag and Commit Verification

When the toolchain fetches a dependency, it resolves the recorded tag to a current commit hash and compares it to the commit recorded in `zane-lock.coda`.

- If the hashes match, fetch proceeds.
- If the hashes differ, the fetch **MUST** abort with a security error.

This detects moved tags and repository tampering. The source checkout **MUST** be the recorded commit, not merely the default branch or an unverified tag checkout. That commit pins `zane-artifacts.coda`; archive verification (§5) extends the pin to the external binary bytes.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#a-tag-for-humans-a-commit-for-the-machine) — "A tag for humans, a commit for the machine" records why both are pinned and why a moved tag is treated as hostile by default.

---

## 5. Fetching

The toolchain fetches the source with Git and checks out the verified commit (§4). It reads that checkout's `zane-artifacts.coda`, selects the row whose target triple exactly matches the requested target, and downloads the recorded URL over HTTPS. HTTPS redirects are allowed; a redirect to another scheme **MUST** be rejected. Direct URLs require no GitHub release-discovery API.

Before extraction, symbol rewriting, or linking, the toolchain **MUST** compute SHA-256 over the downloaded archive bytes and compare it to the committed hash. A mismatch aborts with a security error. The toolchain **MUST NOT** accept a replacement hash obtained from the download host or bypass this check through `--accept-tag-move`; that flag accepts a changed source commit through the explicit update flow (§2.3), whose artifact manifest must still be verified normally.

A missing manifest, missing target row, or unavailable asset fails the prebuilt fetch. An invalid archive or one with no usable object files also fails. These failures do not trigger an automatic source build; source compilation requires explicit opt-in (§12.1). Failed or partial downloads **MUST NOT** become ready cache entries.

Hash verification fixes which bytes the author published. It does not prove that those objects were compiled from the declared source; consuming prebuilts still trusts the author's build.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#release-assets-with-committed-hashes) — "Release assets with committed hashes".

---

## 6. Symbol Versioning

### 6.1 Placeholder-prefix rewriting

A project's library packages are compiled with their exported symbols prefixed by the placeholder marker `!`. During `zane add`, the toolchain rewrites those symbols — replacing the `!` prefix with the resolved version tag, a `%` separator, the package's identity hash, and a second `%` — and places the rewritten binaries into `build/`.

Conceptually:

```zane
!math$vec  →  v1.0.1%3f9a1c02b7e4d6a8%math$vec
```

The **identity hash** is the first 16 hexadecimal digits, in lowercase, of the SHA-256 digest of the UTF-8 encoding of the package's normalized URL — the host-and-path string that also names its cache directory (§7). It is computed from the URL alone, so it is the same for every version of one package and for the HTTPS and SSH spellings of one repository, and it differs between packages at different URLs. The version tag and the identity hash together make a symbol name unique to one version of one package.

The name after the second `%` is the library package's **path** within its project's `lib/` directory, its directory names joined by `.`: `math` for `lib/math/`, `gui.opengl` for `lib/gui/opengl/` ([`packages.md`](packages.md) §2.2). It is baked into the symbol when the project's author compiled it, never the consumer's manifest key, which is a local label (§2.1). Two projects that nickname one library differently therefore link the same symbol, which is what lets the cache share one rewritten artifact between them (§7). The name keeps symbols readable; the identity hash is what tells two packages that share a name apart.

The first `%` delimits the version, because `%` is reserved as the symbol separator and is forbidden in version tags by path-safety validation (§7). The identity hash contains only hexadecimal digits, so the second `%` always follows the first after exactly 16 characters. (`%` is deliberately not `@`, which ELF reserves for symbol versioning and which Mach-O/PE toolchains may reject.)

Distinct packages in one resolved dependency graph **MUST** have distinct identity hashes. The toolchain checks this during dependency resolution and **MUST** abort with an error naming both URLs if two different normalized URLs produce the same identity hash.

The `!` prefix is reserved for this toolchain placeholder role and is not a valid user-defined identifier prefix. The original `!`-prefixed object files come from the verified release archive and are retained under the cache's `artifacts/<target>/build/` directory; rewritten, version-stamped objects are written under `build/<target>/` (§7). Only the fetched library's own placeholder-prefixed exports are rewritten; already-versioned transitive references remain unchanged.

### 6.2 Why rewrite symbols

Rewritten symbol names allow multiple versions of the same package, and different packages that share a name, to coexist in one program without collisions.

### 6.3 Transitive dependencies keep their resolved versions

When a library already depends on another versioned library, the referenced transitive symbols are left as-is. Only the fetched library's own placeholder-prefixed exports are rewritten.

### 6.4 Optional compatibility-based remapping

When a consumer opts in, version-prefixed symbols may additionally be remapped at link time to collapse interchangeable versions of a package onto a single copy. This is layered on the same rewrite step; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#shipping-compiled-objects-and-rewriting-their-symbols) — "Shipping compiled objects, and rewriting their symbols" tells why versioning lives in the linker's namespace, and the separator saga that landed on `%` over `@` and `__`.
> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#two-packages-called-math) — "Two packages called `math`" tells why the symbol carries a hash of the URL rather than the URL, the manifest key, or a globally unique name.

---

## 7. Global Package Cache

Fetched packages are stored in a global cache shared across projects:

```zane
~/.zane/packages/<mangled_url>/<mangled_version>/
  src/
  artifacts/<target>/
    archive.tar.gz
    build/
  build/<target>/
```

The URL and version are mangled into safe path components using Go-style path mangling, after a normalization step that reduces the common Git URL forms to a common host-and-path shape:

1. The URL **scheme** (`https://`, `ssh://`, and the like) is stripped.
2. Any **SSH user prefix** (such as `git@`) is stripped.
3. Any **SCP-style host/path separator** `:` (as in `git@github.com:zane-lang/math`) is normalized to `/`.

Each `/` in the resulting URL then produces a new subdirectory level, so both `https://github.com/zane-lang/math` and `git@github.com:zane-lang/math` normalize to `github.com/zane-lang/math` as nested directories — which also means the HTTPS and SSH forms of one repository share a single cache identity rather than fetching twice. The path-safety check applies to the URL *after* these normalization steps: if the normalized URL or the version tag contains any character that is not safe to use directly as a path component — such as `:`, `@`, `%`, `?`, `#`, or any other character that would be illegal or ambiguous on the host filesystem — `zane add` **MUST** fail immediately with an error rather than attempting to mangle or escape the offending character. (The scheme, the SSH user prefix, and the normalized SCP `:` are exempt by construction; the check screens only the host-and-path remainder that actually becomes directory names.) (`%` is additionally reserved as the symbol separator of §6.1, so forbidding it in tags keeps the boundary after the version unambiguous.) The normalized host-and-path string that names the cache directory is also the input to the identity hash of §6.1, so one cache entry and one symbol identity always correspond.

The `src/` subdirectory holds the repository checked out at the verified commit, including its `lib/` source tree and artifact manifest. `artifacts/<target>/` holds the verified archive and its extracted original objects under the `build/` directory. `build/<target>/` holds the rewritten objects produced during `zane add`. Target triples used as cache directory names **MUST** be single path-safe components.

A ready prebuilt cache entry records the verified source commit, target triple, archive hash, and toolchain tag and verified commit used for rewriting. Reuse requires all recorded values to match the current request; a different target, changed commit, changed hash, or different rewriting toolchain pin invalidates reuse. Source-built objects are kept separately (§12.1). Re-adding a matching package in another project reuses the ready entry without downloading and rewriting it again. The repository URL, not the asset host, determines the package's cache and symbol identity.

The path-safety rules above apply to package identity and cache components, not to artifact URLs. Artifact URLs are validated as HTTPS URLs (§3.1) and are never used directly as filesystem paths.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#one-cache-many-spellings-of-one-url) — "One cache, many spellings of one URL" explains why one URL identity is mirrored into a browsable path, why the HTTPS and SSH spellings normalize together, and why unsafe characters are rejected rather than escaped.

A fully expanded cache path for the `math` example therefore looks like:

```zane
~/.zane/packages/github.com/zane-lang/math/v1.0.1/
  src/
  artifacts/x86_64-unknown-linux-gnu/
    archive.tar.gz
    build/
  build/x86_64-unknown-linux-gnu/
```

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#release-assets-with-committed-hashes) — "Release assets with committed hashes" records how downloaded originals and rewritten objects are kept apart.

---

## 8. Importing a Dependency's Packages

Source code imports a dependency's library package by the package's own name:

```zane
import math
```

And uses its members through that name:

```zane
math$vec(...)
```

`import math` is one of several import forms; which one a file writes fixes how that package's members are spelled at the use site ([`packages.md`](packages.md) §3.3). A project's packages reach the **public library packages** ([`packages.md`](packages.md) §4.3) of its direct dependencies and no others. A package that a dependency reaches only through its own dependencies is imported by adding it to `deps`. Source code never writes version-prefixed names or manifest keys.

Within one project, the project's own top-level library and program packages and the public library packages of its direct dependencies **MUST** have distinct names, and so **MUST** the public library packages of its `test-deps`. Dependency resolution aborts with an error naming both packages and the dependencies that provide them otherwise.

---

## 9. Transitive Dependencies

When a package is fetched, the toolchain recursively reads its `zane.coda` and installs all transitive dependencies needed by that package version before treating the package as ready to link.

A dependency **MUST** have at least one public library package. Resolution aborts with an error naming any dependency that has none. A dependency's program and test packages are never compiled, and a fetched package's `test-deps` block contributes nothing to the graph (§2.1).

> **Story:** [`stories/packages.md`](../stories/packages.md#a-projects-packages-move-into-lib-bin-and-test) — "A project's packages move into `lib/`, `bin/` and `test/`".

---

## 10. Package Dependency Graph

The package dependency graph **MUST** be a directed acyclic graph (DAG). Cyclic imports across package boundaries are not allowed.

If package `A` imports package `B`, then package `B` **MUST NOT** import package `A`, either directly or transitively through any chain of intermediate packages. A package therefore **MUST NOT** import itself, directly or indirectly.

The compiler **MUST** detect and reject cyclic package dependencies at build time with an error message that identifies the cycle.

### 10.1 Single-package mutual references

Within a single package, source files may freely reference each other's declarations. A package is compiled as one unit, so mutual references among declarations in the same package are legal and do not constitute a cycle.

The acyclicity requirement applies only to the package-level dependency graph, not to intra-package references.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#no-cycles-between-packages-every-freedom-within-one) — "No cycles between packages, every freedom within one" explains why cross-package cycles are rejected rather than resolved, and why intra-package mutual reference is left untouched.

---

## 11. Multiple-Version Coexistence

Two packages may depend on different versions of the same upstream library. Because symbol names carry the version tag and identity hash from fetch time (§6.1), both versions may coexist in one final link as long as all references are internally consistent. The same holds for two different upstream libraries that share a package name: their identity hashes differ, so their symbols do too.

This side-by-side coexistence is the default. A consumer may opt into collapsing interchangeable versions onto a single copy via compatibility-based remapping; see [§15 Compatibility Patterns and Remapping](#15-compatibility-patterns-and-remapping).

---

## 12. Platform Artifacts

Packages publish a release archive for each supported target triple and record it in `zane-artifacts.coda` (§3.1). A dependency is usable through the normal prebuilt path only if the verified manifest lists the requested target and its archive is available and passes verification (§5).

### 12.1 Source compilation is explicit opt-in

The normal workflow consumes the verified release artifact from the `artifacts/<target>/build/` directory. A user who does not trust the shipped object file may opt into local compilation from the verified source checkout under `src/lib/` instead, by recording `source` in the dependency's `from` column (§2.1):

```sh
zane add math https://github.com/zane-lang/math v1.0.1 --from-source
```

The choice is part of the committed manifest, so every build of the project compiles that dependency from source. A `source` dependency skips artifact-manifest lookup and release downloads for the selected package. It can therefore build a target with no published artifact. Its transitive dependencies still follow their normal pinned fetch rules. Locally compiled objects undergo the same symbol rewriting (§6), but their results are cached separately under `build-from-source/<target>/`, with the source commit and compilation toolchain tag and verified commit recorded. A source-build request **MUST NOT** be satisfied by a downloaded prebuilt cache entry. This is an explicit trust/debugging escape hatch, not the default package-distribution model.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#where-a-dependencys-code-comes-from) — "Where a dependency's code comes from".

### 12.2 Local path dependencies

A `from` value that is a path names a local project directory. A path that begins with `/` is absolute, and any other path is relative to the root of the project being built. The toolchain compiles that project's library packages from its `lib/` directory on every build instead of fetching the pinned commit. The path project's own `zane.coda` and `zane-lock.coda` supply its dependencies, which follow the normal pinned fetch rules.

```sh
zane dev geometry ../geometry
```

- The compiled objects undergo the symbol rewriting of §6.1 with the dependency's pinned version tag and the identity hash of its locked URL, so they link exactly where the pinned release would.
- Path builds never enter the global package cache (§7).
- A path that does not exist, or holds no `zane.coda`, fails the build with an error naming the key and the path.
- A package whose manifest has a path `from` in a `deps` row cannot be released (§3.2).

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#where-a-dependencys-code-comes-from) — "Where a dependency's code comes from".

---

## 13. Build Flow

At a high level, dependency resolution proceeds in this order:

1. read local `zane.coda` and `zane-lock.coda`, and abort if their keys are out of sync (§2.2)
2. validate the package URLs and version tags for path safety (§7)
3. resolve each recorded tag and verify its commit against the corresponding lock file (§4), for every dependency whose `from` is not a path
4. fetch and check out each verified commit into `~/.zane/packages/<mangled_url>/<mangled_version>/src/`
5. recursively read the dependency manifests of each verified checkout and each path dependency (§12.2), apply the same pin checks, and reject cycles, identity-hash collisions, a dependency with no public library package, or two packages a project reaches under one name (§6.1, §8, §9, §10)
6. for each package, read its committed artifact manifest and select the requested target (§3.1, §12); for a `source` dependency, use §12.1 instead, and for a path dependency, use §12.2 instead
7. reuse a matching ready entry (§7), or download the archive, verify its SHA-256 before extraction, and safely extract its original objects into `artifacts/<target>/build/` (§5)
8. on a prebuilt cache miss, rewrite the library's own `!`-prefixed exports with the resolved version tag and package identity hash, write the results to `build/<target>/`, and mark that cache entry ready only after success; on a matching ready cache hit, use the existing rewritten objects without repeating the rewrite; explicit source compilation follows §12.1 instead
9. for any package listed in the top-level `remaps` block, group the required versions by declared `version-pattern`, collapse interchangeable versions onto the chosen version, and remap displaced references; keep non-interchangeable versions side by side, warning on divergent patterns (see [§15](#15-compatibility-patterns-and-remapping))
10. link the program package being built, with the project's library packages it reaches compiled locally, against the selected target's cached objects, using the separate source-built entry for a `source` dependency and the path build for a path dependency

A test build ([`packages.md`](packages.md) §7.2) runs the same steps with the `test-deps` rows counted among the project's direct dependencies, and links the test package as the program.

---

## 14. Toolchain Version

The `zane-version` field in `zane.coda` pins the toolchain tag used to build the project. It selects the compiler; the reserved `zane` key in `zane-lock.coda` records the commit that tag must resolve to.

- The tag covers the compiler alone. What the compiler supplies is the grammar and the intrinsics — none of which name a declaration in any package ([`syntax.md`](syntax.md) §2.7, [`control-flow.md`](control-flow.md) §4.1) — so pinning it fixes the language without fixing any library.
- **No library is coupled to the toolchain tag, `core` included.** `core` and every other library are ordinary packages, each fetched, versioned, pinned, and remapped like any other dependency, with its own `deps` row in `zane.coda` and entry in `zane-lock.coda`.
- This is why nothing has to preserve backward compatibility across versions. A package that changes incompatibly does not force its consumers forward: versions coexist side by side under version-prefixed symbols (§6, §11), and a project that wants two of them collapsed opts in through `remaps` (§15). That holds for the fundamental types exactly as it holds for anything else — a program may reach two versions of `Int`, and remapping is what collapses them when their compatibility windows say it is safe.
- The reserved `zane` key is subject to the same tag/commit verification as every other entry (§4): a moved toolchain tag is detected, not silently trusted.
- The `zane` command that reads the manifest is not part of the toolchain the tag pins. It installs the compiler the tag names and runs it, so one installed `zane` command builds projects pinned to any toolchain tag.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#core-becomes-part-of-the-language-again) — "`core` becomes part of the language again".
> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#core-becomes-an-ordinary-package-over-a-primitive-floor) — "`core` becomes an ordinary package over a primitive floor".
> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#the-zane-command-is-not-pinned-by-zane-version) — "The `zane` command is not pinned by `zane-version`".

---

## 15. Compatibility Patterns and Remapping

By default, when two parts of the dependency graph require different versions of the same package, both versions are linked side by side using version-prefixed symbols (§6, §11). Compatibility-based remapping is an **opt-in** optimization that collapses such versions onto a single copy when doing so is declared safe, eliminating the duplicate.

> **Story:** [`stories/dependencies.md`](../stories/dependencies.md#opt-in-remapping) — "Opt-in remapping" develops the author/consumer split, why `remaps` names URLs rather than keys, and the unchecked-ABI cost the whole design is built to contain.

### 15.1 Roles: author declares, consumer decides

- **Author (`version-pattern`).** A package author publishes a `version-pattern` in the package's own `zane.coda`. It is **information, not permission**: it declares the range of the package's own versions that are interchangeable at the ABI level. It never forces remapping on or off.
- **Consumer (`remaps`).** The top-level project decides which packages to remap via a `remaps` block listing canonical package URLs. This is the **only** place the remap decision is made. A listed URL may name a direct dependency or a package that appears only transitively — the URL is the canonical identity, so any package in the resolved graph can be named unambiguously even when it has no local key — and listing it needs no version pin, so opting in a transitive package adds no version-management burden. `remaps` blocks in transitively-fetched libraries' manifests are ignored; an intermediate library cannot force a package it depends on to be remapped or kept separate. There is no wildcard or global opt-in: each remapped package is named explicitly in the top-level manifest.

Remapping of a package occurs only when the consumer enables it **and** the published patterns make it ABI-safe. Otherwise the versions coexist unchanged.

### 15.2 Pattern syntax

A `version-pattern` mirrors the shape of the package's version tags, replacing each **numeric** component with a marker:

- `*` — **fixed boundary.** This component must match exactly for two versions to be interchangeable. It carries no priority and does not participate in selection. (Typically the major component.)
- `+` / `-` — **directional and priority-bearing.** `+` means the component is upward-substitutable (a higher value is a valid replacement); `-` means downward-substitutable. The marker is repeated to encode priority.

Markers replace only numeric components. A leading `v` on a tag or pattern (the conventional version prefix) is stripped first; the remaining string is then split into components on `.`. A component is a **marker position** if and only if it consists entirely of one repeated marker character (`*`, `+`, or `-`), for example `*`, `+`, `++`, `-`, or `--`. Any component that contains any other character — text like `rc`, `alpha`, `0-rc`, or digits — is a **literal position** regardless of whether it begins with a marker character: `-rc` within a component is literal, `-` alone is a marker. No escaping is needed or supported; the rule is unambiguous from the component's content alone. So `v*.+.++` strips to `*.+.++` and splits to the marker components `*`, `+`, `++`, matching the major/minor/patch of a tag like `v6.2.9`. A pattern thus has no directional ordering over pre-release identifiers; they participate only as literal matches.

Because components split strictly on `.`, a pre-release identifier joined by another character — `v1.2.3-rc.1`, whose third component is the literal `3-rc` — fuses the patch number into a literal and so cannot be ordered directionally. An author who wants directional remapping to extend over pre-release numbers should therefore separate them with `.` in the tag itself — `v1.2.3.rc.1`, paired with a pattern like `v*.+.++.rc.+` — so the numeric parts land in their own components where a marker can replace them. This is a tag-naming convention, not a toolchain feature: the pattern language treats whatever the `.` split produces, and only `.`-separated numerics are markable.

**Priority is repetition-based: fewer repeats means higher priority** (as with markdown heading levels, where `#` outranks `##`). `+` outranks `++` outranks `+++`. Priority is always explicit — there is no positional default — so a pattern is fully self-describing in isolation.

Example: `v*.+.++` reads as "same major; among interchangeable versions prefer the highest minor first (`+`, top priority), breaking ties by highest patch (`++`)."

#### 15.2.1 Validation rules

- Every `+`/`-` component **MUST** carry an explicit priority via its repetition count.
- No two `+`/`-` components may share a priority level. A pattern with a duplicate level is **rejected at parse time**; this strict total order is what makes selection deterministic.
- `*` components carry no priority and are excluded from the ordering.
- A marker position that contains anything other than `*`, `+`, or `-` is malformed and **MUST** be rejected at parse time. Literal positions must contain only the characters of the tag shape they match.

### 15.3 Selection ("best of both")

With remapping enabled for a package, the toolchain considers the set of versions required across the graph and groups them by their declared `version-pattern` string:

1. Within a group sharing an identical pattern, a candidate **replacement** version may **substitute** for a **required** version (the replacement standing in for the required version's references) when every `*` component is equal and the directional components agree under a **hierarchical** comparison: the `+`/`-` components are examined in priority order (highest priority — fewest repeats — first), and at the first component where the two versions differ, the **replacement's** value must satisfy that component's declared direction relative to the **required's** — strictly greater for `+`, strictly smaller for `-`. Once a higher-priority component satisfies its direction, lower-priority components are unconstrained — so a minor bump that resets the patch to `0` still substitutes. Versions equal in every component substitute trivially.
2. The **chosen** version is the optimum under this priority order — for `+` the greatest value at the highest-priority component, for `-` the least, with lower-priority components breaking ties — provided it may substitute for every other version in the same interchangeable window (those sharing equal `*` components). The strict total order guarantees a single deterministic winner, so the link is reproducible.
3. References to the displaced versions are remapped onto the chosen version and the displaced copies are dropped from the link.

### 15.4 When versions are not interchangeable

- **Same pattern, out of window** (for example, a `*` major component differs): the versions are kept side by side, as in the default model. This is expected and produces **no warning**.
- **Different patterns**: versions of the same package that declare *different* `version-pattern` strings are never remapped onto each other. They are kept side by side and the toolchain emits a **one-time informational warning during dependency resolution** (not on every build, and not a security error) noting that divergent patterns prevented full deduplication. Other versions that do share a pattern still collapse normally.
- **Tag shape mismatch**: a version tag whose structure does not match the package's `version-pattern` — a different number of numeric components, or extra parts such as pre-release identifiers that the pattern's literals do not match — is treated as non-interchangeable. It is never remapped and is kept side by side. This is an expected consequence of heterogeneous tags and produces **no warning**.

### 15.5 Safety: this is an ABI assertion on prebuilt objects

Because libraries ship prebuilt object files (§3, §6), remapping rewrites a caller's symbol references to point at a different version's compiled objects. The author's `version-pattern` therefore asserts **ABI** compatibility across the window — identical signatures, type layouts, and calling conventions — which is a stronger promise than source/API compatibility. A wrong assertion produces silent undefined behavior at link time, with no recompilation to catch it. For this reason remapping is opt-in per consumer — a package is remapped only when listed in `remaps`, and the default is safe coexistence — and it always degrades to safe coexistence when a single common version cannot be shown interchangeable.

### 15.6 Mechanism reuses pull-time rewriting

Remapping is a link-time pass layered on the symbol rewriting of §6.1. Exact pins are untouched: every required version — direct or transitive — remains recorded in the `zane.coda` / `zane-lock.coda` of the package that depends on it (§2.2) and is fetched. The pass only chooses which cached objects to link and rewrites the displaced references — conceptually `v6.2.9%3f9a1c02b7e4d6a8%math$vec → v6.3.4%3f9a1c02b7e4d6a8%math$vec` — onto the chosen version. Only the version tag changes: every version of one package shares its identity hash, so the rewrite never moves a reference from one package to another.
