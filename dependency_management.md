# Zane Dependency Management

This document specifies how Zane identifies, fetches, versions, caches, and links external packages.

---

## 1. Overview

Zane treats a package's full source URL as its identity and pins every dependency to an exact tag plus commit hash.

- **`URL identity`.** The repository URL is canonical; local aliases are only conveniences.
- **`Exact versioning`.** Each dependency records a specific tag and the commit that tag must resolve to.
- **`Prebuilt distribution`.** Libraries are distributed as source plus prebuilt object files committed to the repository.
- **`Global caching`.** Fetched and versioned artifacts are shared across projects on the machine.

---

## 2. Manifest

Every project has a `zane.coda` manifest with a `deps` block:

```
deps [
    alias url version commit
    math https://github.com/zane-lang/math v1.0.1 a3f8c2d
]
```

Each row records:

- **alias**: the local import key used in source code
- **url**: the canonical package identity
- **version**: the exact tag requested by the user
- **commit**: the exact commit that tag must resolve to

Users update the manifest through CLI commands rather than by manual editing.

### 2.1 Commit hashes are recorded and updated by commands
`zane add` resolves the requested tag to its current commit hash and writes both the tag and commit into `zane.coda`. The user does not type the commit hash manually in the normal workflow.

`zane update alias version` replaces both the recorded tag and the recorded commit for that alias. A whole-project update re-resolves each dependency and refreshes both fields.

If a tag has moved and the user intentionally wants to trust the new commit, the update flow must require an explicit override flag rather than silently refreshing the hash.

---

## 3. Repository Layout

Library repositories include prebuilt objects in the git tree:

```
math/
  src/
  prebuilt/
  zane.coda
```

The toolchain selects the correct prebuilt artifact for the current target triple.

---

## 4. Tag and Commit Verification

When the toolchain fetches a dependency, it resolves the recorded tag to a current commit hash and compares it to the manifest.

- If the hashes match, fetch proceeds.
- If the hashes differ, the fetch **MUST** abort with a security error.

This detects moved tags and repository tampering.

---

## 5. Fetching

Fetching is a normal git fetch/clone against the package URL. Because binaries live in the repository tree, Zane does not depend on host-specific release-asset APIs.

If the required target artifact is missing from `prebuilt/`, the fetch fails for that target.

---

## 6. Symbol Versioning

### 6.1 Placeholder-prefix rewriting
Libraries are compiled with their own exported symbols prefixed by the placeholder marker `!`. During `zane add`, the toolchain rewrites those symbols to include the resolved version tag.

Conceptually:

```
!math$vec  →  v1.0.1math$vec
```

The `!` prefix is reserved for this toolchain placeholder role and is not a valid user-defined identifier prefix. Only the fetched library's own placeholder-prefixed exports are rewritten; already-versioned transitive references remain unchanged.

### 6.2 Why rewrite symbols
Versioned symbol names allow multiple versions of the same package to coexist in one program without collisions.

### 6.3 Transitive dependencies keep their resolved versions
When a library already depends on another versioned library, the referenced transitive symbols are left as-is. Only the fetched library's own placeholder-prefixed exports are rewritten.

---

## 7. Global Package Cache

Fetched packages are stored in a global cache shared across projects:

```
~/.zane/packages/<mangled_url>/<mangled_version>/
```

The URL is mangled into a safe path. Re-adding the same package version in another project reuses the existing cached artifact rather than downloading and rewriting it again.

---

## 8. Import Syntax

Source code imports by manifest alias:

```zane
import math
```

And uses package members through that alias:

```zane
math$vec(...)
```

Source code never writes version-prefixed package names directly. The compiler resolves aliases through `zane.coda`.

---

## 9. Transitive Dependencies

When a package is fetched, the toolchain recursively reads its `zane.coda` and installs all transitive dependencies needed by that package version before treating the package as ready to link.

---

## 10. Multiple-Version Coexistence

Two packages may depend on different versions of the same upstream library. Because symbol names are version-prefixed at fetch time, both versions may coexist in one final link as long as all references are internally consistent.

---

## 11. Platform Artifacts

Packages may ship multiple prebuilt object files for different target triples. A dependency is usable on a target only if the repository contains the matching prebuilt artifact for that target.

### 11.1 Source compilation is explicit opt-in
The normal workflow consumes the repository's checked-in prebuilt artifact. A user who does not trust the shipped object file may opt into local compilation from the verified source checkout instead.

```sh
zane add math https://github.com/zane-lang/math v1.0.1 --from-source
```

This is an explicit trust/debugging escape hatch, not the default package-distribution model.

---

## 12. Build Flow

At a high level, dependency resolution proceeds in this order:

1. read local `zane.coda`
2. resolve each tag to its current commit hash
3. verify commit hashes against the manifest
4. fetch missing packages
5. rewrite placeholder-prefixed exports with the resolved version
6. install into the global cache
7. link the locally compiled program against cached dependency artifacts

---

## 13. Design Rationale

| Decision | Rationale |
|---|---|
| URL as package identity | Avoids global registry naming conflicts and matches how code is actually fetched. |
| Exact tag plus commit hash | Keeps builds reproducible and makes moved tags detectable. |
| Prebuilt objects in the repository | Keeps fetching host-agnostic and lets source and artifacts share one immutable commit identity. |
| Pull-time symbol versioning | Enables multiple versions to coexist without requiring source-level version names. |
| Global package cache | Avoids repeated downloads and rewrite work across projects. |
| Alias-based imports | Keeps source code readable while the manifest remains the single source of truth for version resolution. |
