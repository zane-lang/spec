# Zane Concurrency Model

This document specifies Zane's concurrency model: compiler-managed parallelism, the `spawn` keyword, and the safety rules that govern concurrent execution.

> **See also:** [`purity.md`](purity.md) §4 for effect levels. [`memory_model.md`](memory_model.md) §4 for lifetime rules. [`syntax.md`](syntax.md) §4 for `spawn` syntax.

---

## 1. Overview

Zane separates **parallelism** (compiler-managed, unobservable) from **concurrency** (explicit, programmer-controlled).

- **`Implicit parallelism`.** The compiler may run provably independent work in parallel when it cannot change program results.
- **`Explicit concurrency`.** `spawn` starts a concurrent function call; ordering is the programmer’s responsibility.
- **`Water-tower lifetimes`.** A scope’s owned objects live until all spawned work in that scope completes.
- **`Single-writer rule`.** At most one concurrent `mut` accessor may exist for any object.
- **`No async coloring`.** Concurrency is chosen at the call site rather than encoded into function signatures.

---

## 2. Implicit Parallelism (Compiler Responsibility)

### 2.1 Compile-time reduction
All **Total Pure** functions with statically known inputs are evaluated at compile time. This removes them from the runtime graph entirely.

### 2.2 Parallelization of the residual graph
After compile-time reduction, the compiler analyzes the remaining work for independence. It may schedule independent work in parallel when:

- the effect system proves the work is non-conflicting
- parallelization is profitable (based on instruction count, IO presence, or other heuristics)

This parallelism is **unobservable**: it must not change output, only timing.

### 2.3 Total Pure and Pure are distinct
The compiler distinguishes:

- **Total Pure**: no side effects and guaranteed termination, so compile-time evaluation is legal when inputs are known
- **Pure**: no side effects, but termination is not proven, so the call remains runtime work even though it is still parallelizable

This distinction matters for compile-time reduction, not for the legality of runtime parallelism. See [`purity.md`](purity.md) §3 and §9 for the effect-level definitions and matrix.

### 2.4 Thread configuration
The runtime uses a work-stealing thread pool configured by `@threads`:

```zane
@threads(8)
@threads(auto)
```

`auto` maps to hardware concurrency at startup. The thread count is fixed for a program’s lifetime unless the standard library exposes a dedicated, explicitly documented runtime override.

---

## 3. `spawn` and Explicit Concurrency

### 3.1 `spawn` targets function calls only
`spawn` starts a concurrent **function call**. It is illegal on blocks or control flow.

```zane
spawn runServer(8080)        // ok
spawn [runServer(8080)]      // ILLEGAL
spawn if cond { f() }        // ILLEGAL
```

### 3.2 Spawned values block on read
A spawned call that returns a value can bind to a symbol. Reading that symbol blocks until the spawned call returns.

```zane
result String = spawn listen(8080)
print(result) // blocks until listen returns
```

### 3.3 Abortable spawned calls are handled at the spawn site
If a spawned call is abortable, it must still attach `?` or `??` directly to the `spawn` expression. There is no separate propagation form at result-read time.

```zane
port Int = spawn listen(8080) ? err {
    resolve Int(404)
}

fallback Int = spawn listen(8081) ?? Int(404)
```

The bound symbol has the handled primary type, and reading it still blocks until the spawned call finishes.

### 3.4 Stalling functions are parked
A stalling function called without `spawn` blocks the caller. A stalling function called with `spawn` is parked by the runtime so it does not consume a worker thread while waiting.

### 3.5 Stalling without `spawn` is ordinary blocking
A stalling function does not require `spawn`. When called normally, it simply blocks the current execution until it returns.

### 3.6 Ordering is explicit
The compiler does not reorder `spawn` calls. The order in the source is the order in which spawns are started, and any blocking read happens exactly where written.

Independent work may still be parallelized only when doing so preserves those source-visible points.

### 3.7 No serial-equivalence guarantee
`spawn` explicitly opts out of serial equivalence. Program results may depend on scheduling except where constrained by effect and ownership rules.

---

## 4. Safety Rules Under Concurrency

### 4.1 Water-tower lifetime extension
A scope does not complete until all `spawn`ed calls inside it have completed. Owned objects in that scope are destroyed only when the scope is **drained**.

### 4.2 Single-writer rule for object mutation
For any object, at most one concurrent `mut` accessor is allowed. Two `spawn`ed calls that both require `mut` access to the same object are a compile-time error.

### 4.3 Effect conflicts on external resources
The effect system classifies resource access as **read** or **write**. Concurrent accesses are permitted only when they do not conflict:

- read/read: allowed
- read/write: serialized
- write/write: serialized

The compiler enforces this from effect signatures; the programmer does not add locks.

### 4.4 Refs passed to spawned work are copied at spawn time
When a `ref` is passed to a spawned call, the callee receives a copy of that ref value. Rebinding the caller's `ref` symbol later changes only the caller's storage; it does not retarget the copy already held by spawned work.

---

## 5. Concurrency Boundaries

### 5.1 No cancellation or shutdown ordering
The language does not provide cancellation, kill groups, or shutdown ordering. A spawned function runs until it returns or the process is terminated externally.

### 5.2 Lambdas do not capture
Lambdas (and blocks used as values) **MUST NOT** capture outer variables. All dependencies must be passed explicitly. This keeps effect tracking and single-writer verification tractable.

### 5.3 No `async`/`await` syntax
Zane does not define `async` or `await`. Concurrency is expressed only through `spawn` and compiler-managed parallelism. This avoids function coloring: a function does not become a different kind of thing merely because some caller chooses to run it concurrently.

### 5.4 No language-level process or channel abstraction
Zane does not define a dedicated `Process` type, actor primitive, or channel primitive in the core language. Long-running concurrent work is expressed as ordinary spawned function calls plus explicit state flow governed by ownership and effect rules.

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Separate parallelism from concurrency | Parallelism must be unobservable; concurrency changes program meaning and must be explicit. |
| No `async`/`await` function coloring | Whether work runs concurrently is a call-site choice, not a permanent property of a function definition. |
| `spawn` on function calls only | Keeps conflict detection purely signature-based and statically decidable. |
| Park stalled spawned work | Lets long-waiting calls coexist with a bounded worker pool without tying up OS threads unnecessarily. |
| Water-tower lifetimes | Extends safe lifetimes into concurrent work without GC. |
| Spawned values block on read | Makes data dependencies explicit at the point of use. |
| Abortable spawns are handled at the spawn site | Keeps `spawn` in the same mandatory-handling model as ordinary calls. |
| No cancellation/shutdown | Avoids hidden control flow; responsibility stays with the programmer. |
| No lambda capture | Prevents hidden dependencies that would undermine effect analysis. |
| No language-level process grouping | Keeps the concurrency core minimal and leaves orchestration to ordinary program structure. |

---

## 7. Summary

| Concept | Rule |
|---|---|
| Implicit parallelism | Compiler may parallelize only when results are unchanged |
| `spawn` | Starts a concurrent function call; blocks only when results are read |
| Abortable `spawn` | Must attach `?` or `??` directly to the spawn expression |
| Water tower | A scope exits only after all spawned work completes |
| Mutation | At most one concurrent `mut` accessor per object |
