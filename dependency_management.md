# Zane Dependency Management

This document specifies how Zane resolves, fetches, versions, and links external packages.

---

## 1. Overview

Zane treats package URLs as the canonical identity and pins dependencies to exact tags plus commit hashes.

- **`URL identity`.** The full repository URL is the package identity.
- **`Exact versioning`.** Dependencies are pinned to exact tags and verified against commit hashes.
- **`Prebuilt objects`.** Libraries are distributed as precompiled object files committed to the repository.

---

## 2. Package Identity

Packages are identified by their full source URL (e.g., a GitHub or Codeberg repository). Short names are local aliases only.

---

## 3. Manifest (`zane.coda`)

Every project has a `zane.coda` manifest containing a `deps` block:

```
deps [
    alias url version commit
    math https://github.com/zane-lang/math v1.0.1 a3f8c2d
]
```

Each entry includes:

- **alias**: the local import key
- **url**: the repository URL
- **version**: an exact tag
- **commit**: the tag’s commit hash at resolution time

The manifest is updated only by CLI commands.

---

## 4. CLI Commands

```
zane add <alias> <url> <tag>
zane remove <alias>
zane update <alias> <tag>
zane update
```

`zane add` resolves the tag to a commit hash and writes both fields to the manifest. `zane update` re-resolves tags to their current commits.

---

## 5. Repository Layout

Library repositories include prebuilt object files alongside source:

```
math/
  src/
  prebuilt/
  zane.coda
```

The toolchain links against the prebuilt objects unless explicitly configured otherwise.

---

## 6. Tag and Commit Verification

When fetching a dependency, the toolchain resolves the tag to a commit hash and compares it to the manifest. If the hashes differ, the fetch **MUST** abort with a security error.

---

## 7. Design Rationale

| Decision | Rationale |
|---|---|
| URL as package identity | Avoids global registry naming collisions. |
| Exact tags + commit hashes | Guarantees reproducible builds and detects tag rewrites. |
| Prebuilt objects in repo | Makes dependency use fast and deterministic. |
