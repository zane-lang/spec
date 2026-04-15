# Zane Dependency Management

## Overview

Zane's dependency management system has no central registry. A package is identified by its source repository URL — a GitHub or Codeberg URL is the canonical package identity. Version information and dependency relationships live entirely in a per-project manifest file (`.coda`); source code never contains URLs or version numbers.

The `import` statement takes only a bare identifier. That identifier is an alias resolved to a URL and version by the manifest. Multiple projects can alias the same package under different names without ambiguity, and source code is insulated from version details entirely.

There is no global package database, no account required, and no approval process. If a repository is publicly reachable, it is a valid Zane dependency.

---

## Manifest format

Every Zane project has a `.coda` file at its root. The format uses bare keywords, one declaration per line, matching the style of the language itself.

```
package myapp
version 1.0.0

dep plot  github.com/zane-lang/plot  4.2.1
dep http  github.com/zane-lang/http  3.0.7
```

Each `dep` line has three fields:

| Field | Description |
|---|---|
| alias | The bare identifier used in `import` statements and as the `$` prefix in source code |
| url | The canonical package identity — the repository URL, no scheme prefix |
| resolved-version | The exact fetched version; written and maintained by the toolchain |

The alias must be a valid Zane identifier (`[\p{L}_][\p{L}\p{N}_]*`). The resolved version is a full semver triple (`MAJOR.MINOR.PATCH`) written by `zane add` or `zane update`; it is not written by hand.

### Mutating the manifest

The manifest is managed through three CLI commands. Direct edits to the `dep` lines are valid but the toolchain normalises the file on the next run.

```
zane add   github.com/zane-lang/plot 4     # fetch latest 4.x.x, append dep line
zane remove plot                           # remove the dep line for alias 'plot'
zane update plot                           # fetch latest MAJOR.x.x, rewrite resolved version
```

`zane add` derives the alias from the final path component of the URL unless a `--alias` flag overrides it. If a dep with that alias already exists, the command errors.

`zane update` without an argument updates all deps.

---

## Versioning model

Zane pins the **major version only**. When you add a dependency with `zane add github.com/zane-lang/plot 4`, the toolchain fetches the latest `4.x.x` release at that moment and writes the full triple to the manifest for reproducibility. Future `zane build` invocations use the pinned triple exactly. Only an explicit `zane update` re-resolves to a newer `4.x.x`.

The model rests on a semver trust contract:

- **Patch** releases are bug fixes — no API changes.
- **Minor** releases add to the API — existing callers are unaffected.
- **Major** releases are allowed to break API compatibility.

Pinning only the major means a team always gets bug fixes and new features for free, while API-breaking changes require an explicit update to a new major. The full resolved version in the manifest means builds are reproducible across machines and CI runs without a lock file format separate from the manifest.

Different major versions of the same package are treated as **distinct packages** at the symbol level. A project may depend on both `plot` v1 and `plot` v2 simultaneously if two of its deps require them — they do not conflict. See Symbol mangling and Transitive conflict scenarios.

---

## Import syntax

Every Zane source file begins with a `package` declaration followed by zero or more `import` declarations:

```antlr
pkgDef    : 'package' name=IDENTIFIER ;
pkgImport : 'import'  name=IDENTIFIER ;
```

The `import` statement takes the alias exactly as it appears in the manifest, with no URL or version:

```zane
package myapp

import plot
import http
```

Symbols from an imported package are accessed using the `$` separator defined by the grammar:

```antlr
typeSymbol  : (package=IDENTIFIER '$')? name=IDENTIFIER ;
valueSymbol : (package=IDENTIFIER '$')? name=IDENTIFIER ;
```

The package identifier comes first, then `$`, then the symbol name:

```zane
plot$Canvas canvas = plot$Canvas(800, 600)
plot$draw(canvas, scene)

http$Response resp = http$get("https://example.com")
```

Without a package prefix, a symbol refers to the current package. The `$` separator is the only namespace separator in Zane — there is no `::` or `.` equivalent for cross-package references. The `!` character is an operator character (part of the `OPERATOR` lexer token) and has no role in namespacing.

---

## Symbol mangling

At the source level, the package separator is `$` as defined by the grammar. No version number ever appears in source.

At the **object-file level**, the compiler and toolchain need to distinguish symbols from different major versions of the same package, because the same major version from two separate dependency paths is deduplicated (same symbols), while different major versions must coexist without collision.

The convention is:

- Symbols in compiled object files are mangled as `pkgname_MAJOR$symbolname`.
- For example, major version 1 of `plot` produces symbols `plot_1$draw`, `plot_1$Canvas`, etc.
- Major version 2 produces `plot_2$draw`, `plot_2$Canvas`, etc.

This is a **below-source-level** concern. The `pkgname_MAJOR` prefix is not valid as a user-facing import alias — `IDENTIFIER` (`[\p{L}_][\p{L}\p{N}_]*`) does allow digits after the first character (so `plot_1` is syntactically valid), but versioned identifiers like `plot_1` are **prohibited by convention** as import aliases. Import aliases must be plain package names with no embedded version number. The mangled form `plot_1` exists only in object-file symbol tables and is never written in source code.

Source code is always compiled with knowledge of which major version each import alias resolves to. The compiler emits references to the mangled names directly — `plot$draw(...)` in source becomes a reference to `plot_1$draw` in the object file if `plot` resolves to major 1.

### Library distribution

Pre-compiled library objects are distributed with a **version placeholder** in the mangled symbol names rather than a hard-coded major number. This allows a single set of pre-compiled objects to be re-used after symbol substitution at link time regardless of which major the consumer resolves to.

The placeholder is the string `_$` in the package-name portion of each symbol. For example, the distributed object file for `plot` contains symbols like:

```
plot_$draw
plot_$Canvas
plot_$Rect
```

The `_$` token is the version placeholder. This is not Zane source syntax — it is a binary-level naming convention that the Zane toolchain knows about. The `$` character is legal in ELF and COFF symbol names even though it is not legal in a Zane `IDENTIFIER`.

---

## Symbol substitution pass

Before linking, the build system applies a substitution pass to each pre-compiled library object. It rewrites every placeholder symbol to the major-versioned form resolved for this build:

```
llvm-objcopy --redefine-sym plot_$draw=plot_1$draw      \
             --redefine-sym plot_$Canvas=plot_1$Canvas  \
             --redefine-sym plot_$Rect=plot_1$Rect      \
             libplot.o libplot_v1.o
```

The resulting `libplot_v1.o` exports `plot_1$draw`, etc., and is linked into the final binary. The original distributed `libplot.o` is unchanged in the cache.

The toolchain generates the full `--redefine-sym` argument list by reading the exported symbol table of the cached library object with `llvm-nm` and filtering for symbols matching the `pkgname_$` prefix. No hand-written symbol lists are required.

### Platform artifacts

Pre-compiled objects are platform-specific. The naming convention is:

```
libplot-4.2.1-x86_64-linux-gnu.o       # ELF  (Linux, x86-64)
libplot-4.2.1-aarch64-linux-gnu.o      # ELF  (Linux, ARM64)
libplot-4.2.1-x86_64-windows-msvc.obj  # COFF (Windows, x86-64)
```

The format is `lib{alias}-{version}-{target-triple}.{ext}` where the extension is `.o` for ELF targets and `.obj` for COFF targets. The Zane toolchain uses the Zig compiler infrastructure to cross-compile library objects for any supported target triple.

Library authors are expected to distribute pre-compiled objects for all supported targets in their repository's release assets. A standard CI script pattern:

```
for each TARGET in supported-targets:
    zig build-obj --target TARGET src/lib.zane -o lib{pkg}-{version}-{TARGET}.{ext}
    sign and attach to release
```

If no pre-compiled object exists for the host triple, the toolchain falls back to compiling the library source directly.

---

## Fetch and cache

Fetched packages are stored in a local cache keyed by URL, full version, and target triple:

```
~/.zane/cache/
  github.com/zane-lang/plot/4.2.1/x86_64-linux-gnu/
    libplot.o                    # distributed object (placeholder symbols)
    libplot.zane-symbols          # exported symbol list (generated by llvm-nm)
  github.com/zane-lang/http/3.0.7/x86_64-linux-gnu/
    libhttp.o
    libhttp.zane-symbols
```

A cached entry is reused without network access as long as:

- The resolved version in the manifest matches the directory name exactly.
- The target triple matches the current build target.

`zane build` never re-fetches a dep that already exists in the cache for the current target. `zane update` explicitly re-resolves and re-fetches.

The `.zane-symbols` file is a plain-text list of the mangled symbol names exported by the library object — one symbol per line. It is generated once at fetch time and used at every build to construct the `--redefine-sym` arguments without re-running `llvm-nm`.

---

## Dependency graph topology

Zane models the dependency graph as a **tree**, not a directed acyclic graph (DAG).

In a DAG model, the same package node is shared across all paths that reach it. In Zane's tree model, each dependency edge owns its own subtree. If two packages both depend on `plot 4.x.x`, the tree appears to have two separate `plot` nodes — but because they resolve to the same major version, they produce identically-named symbols (`plot_4$draw`, etc.), and the linker deduplicates them automatically. No explicit deduplication mechanism is needed; it falls out of the symbol naming convention.

If two packages depend on different major versions — `plot 1.x.x` and `plot 2.x.x` — the symbols are distinct (`plot_1$draw` vs `plot_2$draw`) and both link into the final binary without collision. The tree structure means each edge independently tracks which version it resolved to, so per-edge substitution (see Transitive dependency resolution) is straightforward.

### Why trees instead of a DAG

The main consequence of the tree model is that **diamond dependencies are resolved per-edge, not globally**. If packages A and B both depend on `util`, A and B each get their own resolved `util` instance in the dependency tree. If both resolve to the same major version, deduplication happens at the linker level for free. If they resolve to different major versions, isolation happens at the linker level for free.

There is no global resolution pass, no version conflict resolution algorithm, and no error when two paths reach different minor versions of the same major. The tree topology eliminates the diamond problem entirely by refusing to share nodes — sharing happens at the symbol level as an emergent property instead.

---

## Transitive dependency resolution

Each library's transitive dependencies are resolved **in the library's own namespace**, not in the consuming project's namespace. The consuming project's manifest controls only its direct dependencies.

When library `plot 4.2.1` is fetched, the toolchain also fetches all of `plot`'s transitive dependencies at the versions `plot` recorded in its own manifest. The symbol substitution pass is applied **per-edge**:

1. Fetch `plot 4.2.1` and its dep list.
2. For each of `plot`'s deps, fetch and apply substitution with `plot`'s resolved versions — not the consumer's.
3. Link `plot`'s substituted objects into `libplot_resolved.o` for the consumer.

The consumer links against `libplot_resolved.o`. It never directly interacts with `plot`'s transitive dependencies. Their symbols may be present in the binary (if not already provided by another dep), but they are invisible at the source level to the consumer.

This per-edge model means a library always sees the symbol versions it was compiled against, regardless of what the consumer has chosen for its own direct deps. Two libraries that both use `util` but at different minor versions are both satisfied independently — their respective `util` symbols are substituted and resolved in their own sub-trees.

---

## Build flow

A complete build proceeds in these steps:

```
1. Parse .coda — read all dep aliases, URLs, and resolved versions.

2. For each dep (recursively, depth-first):
   a. Check the local cache (~/.zane/cache/) for the resolved version and target triple.
   b. If absent, fetch the release asset for the matching target triple.
   c. Generate the .zane-symbols file if not already cached.

3. Detect circular dependencies — if any dep path reaches the same URL at any ancestor level,
   abort with an error naming the cycle.

4. For each dep, apply the symbol substitution pass:
      llvm-objcopy --redefine-sym <placeholder>=<versioned> ...
   producing a versioned object file in a per-build scratch directory.

5. Compile all project source files. The compiler resolves each import alias to its
   major version from the manifest and emits mangled symbol references accordingly.

6. Link all compiled project objects and all versioned dep objects into the final binary.

7. Clean up the per-build scratch directory.
```

Step 3 is a hard error — circular dependencies are not supported. Unlike different-major isolation (which works automatically) or same-major deduplication (which works automatically), cycles cannot be resolved by any symbol-level mechanism and indicate a structural error in the dependency graph.

---

## Transitive conflict scenarios

### Same major version — deduplication

If packages A and B both declare `dep util github.com/zane-lang/util 2.x.x` and both resolve to `util 2.3.1`, the symbol substitution pass for each produces identically-named symbols (`util_2$something`). The linker deduplicates these automatically. One copy of `util`'s code ends up in the binary. No explicit deduplication mechanism is required.

If A resolves to `util 2.3.1` and B resolves to `util 2.5.0`, the symbol names are still `util_2$something` for both — different minor and patch versions produce the same mangled names under the same major. The linker picks one copy. Which copy wins is unspecified; the build system may warn. This is the accepted cost of semver trust: minor versions are assumed compatible, and the minor version in the manifest is advisory rather than enforcement.

### Different major versions — isolation

If A depends on `util 1.x.x` and B depends on `util 2.x.x`, the substitution pass produces `util_1$something` for A and `util_2$something` for B. These are distinct symbols. Both link into the binary without conflict. Each library sees the version it was compiled against. No action is required by the project author.

### Circular dependency — hard error

If A depends on B and B depends on A (directly or transitively), the build aborts:

```
error: circular dependency detected
  myapp -> A -> B -> A
```

Circular dependencies cannot be resolved by symbol substitution and indicate a design flaw in the dependency graph. They must be broken by refactoring shared code into a third package that neither A nor B depends on.

---

## Design decisions

| Decision | Rationale |
|---|---|
| No central registry | Eliminates a single point of failure and gatekeeping. Any public repo is a valid dep. |
| URL as package identity | Globally unique without coordination. Repo ownership implies package ownership. |
| Major-only pinning | Minor and patch updates are semver-safe. Pinning full versions creates unnecessary churn. |
| Full version written to manifest | Reproducible builds without a separate lock file. The manifest is both the config and the lock. |
| Tree topology, not DAG | Eliminates global diamond resolution. Deduplication and isolation emerge from symbol naming. |
| Per-edge substitution | Each library's transitive deps are resolved in its own namespace; consumers are insulated. |
| `$` as source-level separator | Matches the grammar (`typeSymbol`, `valueSymbol`). Naturally partitions the symbol namespace. |
| Version in mangled ABI names | Different major versions coexist in one binary with no runtime overhead. |
| Pre-compiled objects | Fast consumer builds — only the project's own source is compiled, not all transitive deps. |
| `llvm-objcopy` substitution | Standard tool, no custom binary format. Works on both ELF and COFF. |
| Placeholder `_$` in distributed objects | Library authors distribute one set of objects; the toolchain stamps in the major version at link time. |
| Circular deps as hard errors | No sound resolution exists. Fail early with a clear diagnostic. |
| Cache keyed by URL + version + triple | Fetches are idempotent. Network access occurs only when a new version or target is needed. |
