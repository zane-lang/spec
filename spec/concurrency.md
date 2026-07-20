# Zane Concurrency Model

This document specifies Zane's concurrency model: compiler-managed parallelism, the `spawn` keyword, and the safety rules that govern concurrent execution.

> **See also:** [`effects.md`](effects.md) §3 for effect levels. [`lifetimes.md`](lifetimes.md) §2 for lifetime rules. [`syntax.md`](syntax.md) §4 for `spawn` syntax.

---

## 1. Overview

Zane separates **parallelism** (compiler-managed, unobservable) from **concurrency** (explicit, programmer-controlled).

- **`Implicit parallelism`.** The compiler may run provably independent work in parallel when it cannot change program results.
- **`Explicit concurrency`.** `spawn` starts a concurrent function or method call; ordering is the programmer’s responsibility.
- **`Water-tower lifetimes`.** A scope’s owned objects live until all spawned work in that scope completes.
- **`Mutation needs a value receiver`.** A spawned call may mutate only a value-typed receiver; a value type's transitive alias-freedom lets the compiler rule out a data race from the receiver's type, and at most one spawn may mutably borrow a given location.
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

This distinction matters for compile-time reduction, not for the legality of runtime parallelism. See [`effects.md`](effects.md) §3 and §9 for the effect-level definitions and matrix.

### 2.4 Thread configuration
The runtime uses a work-stealing thread pool configured by `@threads`:

```zane
@threads(8)
@threads(auto)
```

`auto` maps to hardware concurrency at startup. The thread count is fixed for a program’s lifetime unless the standard library exposes a dedicated, explicitly documented runtime override.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#parallelism-you-cant-see-concurrency-you-must-ask-for) — "Parallelism you can't see, concurrency you must ask for".

---

## 3. `spawn` and Explicit Concurrency

### 3.1 `spawn` targets function and method calls only
`spawn` starts a concurrent **function or method call**. Both forms expose a verb signature for effect and conflict analysis. It is illegal on blocks or control flow.

```zane
spawn runServer(8080)            // ok: function call
spawn server:listen(8080)         // ok: read-only method call
spawn server!refreshConnections() // ok: mutating method call
spawn [runServer(8080)]          // ILLEGAL
spawn if cond { f() }            // ILLEGAL
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

### 3.4 Stalling calls are parked
A stalling function or method called without `spawn` blocks the caller. The same call made with `spawn` is parked by the runtime so it does not consume a worker thread while waiting.

### 3.5 Stalling without `spawn` is ordinary blocking
A stalling function or method does not require `spawn`. When called normally, it simply blocks the current execution until it returns.

### 3.6 Ordering is explicit
The compiler does not reorder `spawn` calls. The order in the source is the order in which spawns are started, and any blocking read happens exactly where written.

Independent work may still be parallelized only when doing so preserves those source-visible points.

### 3.7 No serial-equivalence guarantee
`spawn` explicitly opts out of serial equivalence. Program results may depend on scheduling except where constrained by effect and ownership rules.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#spawn-and-why-it-marks-only-a-call) — "`spawn`, and why it marks only a call".

---

## 4. Safety Rules Under Concurrency

### 4.1 Water-tower lifetime extension
A scope does not complete until all `spawn`ed calls inside it have completed. Owned objects in that scope are destroyed only when the scope is **drained**.

The analogy is a vertical water tower with water at the top and one horizontal plate for each still-running spawned call in that scope. The water cannot fall past a plate that is still in place, so destruction cannot pass that still-live concurrent work either.

Each time one spawned call finishes, one plate is removed. The water level drops to the next remaining plate. Only when the last plate is gone does the water reach the bottom of the tower. That moment is when the scope drains and ordinary destruction runs.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#the-water-tower-lifetimes-that-survive-the-spawn) — "The water tower: lifetimes that survive the spawn".

### 4.2 Concurrent mutation requires a value-typed receiver
A spawned call may **mutate** state only through a value-typed receiver. A spawned `mut` call whose receiver is a reference type (a `#`-marked type) is a compile-time error. The rule is sound because a value type is transitively alias-free — it contains no reference-type or `&` field anywhere downstream (see [`memory.md`](memory.md) §2.10) — so no two names can reach the same mutated object by different paths. The compiler therefore rules out an aliased data race from the receiver's *type* alone, with no whole-program alias analysis.

A direct consequence is that reference types are never mutated by spawned work, so every concurrent **read** of the reference-typed object graph is safe by construction.

### 4.3 Single writer per storage location
For any one storage location, at most one live spawned call may hold a **mutable borrow** — the `!` receiver of a spawned `mut` call. Two spawned calls that mutably borrow the same location are a compile-time error. Because value types carry no `&`, a location's identity is unambiguous — there is no hidden alias to obscure that two receivers denote the same slot — so this disjointness is checked at the spawn site by inspecting the receivers, not by tracing the program. The owning scope may not access a location while a live spawn holds its mutable borrow; the borrow is released when that spawn completes (§4.1).

### 4.4 Reads take a coherent snapshot
A spawned call may read a value that another live spawn is mutating; the read observes a **coherent snapshot** of the value rather than blocking. Reading a shared value into a fresh binding — `snap VarType = shared` — is what takes the snapshot, and the copy is tear-free even when the writer is mid-update. This replaces lock-based serialization for in-memory value state, so a real-time reader never waits on a writer. Serialization still applies to external, capability-backed resources (§4.5).

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#value-typed-mutation-closing-the-aliased-write-gap) — "Value-typed mutation: closing the aliased-write gap".

### 4.5 Effect conflicts on external resources
The effect system classifies resource access as **read** or **write**. Concurrent accesses to external, capability-backed state are permitted only when they do not conflict:

- read/read: allowed
- read/write: serialized
- write/write: serialized

The compiler enforces this from effect signatures; the programmer does not add locks.

### 4.6 Tethers passed to spawned work remain independent
When a tether is passed to a spawned call, the callee receives its own tether to the same owner. Rebinding the caller's `&` symbol later changes only the caller's storage; it does not retarget the tether already held by spawned work.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#safety-the-compiler-proves-from-signatures-not-locks) — "Safety the compiler proves from signatures, not locks".

---

## 5. Concurrency Boundaries

### 5.1 No cancellation or shutdown ordering
The language does not provide cancellation, kill groups, or shutdown ordering. A spawned call runs until it returns or the process is terminated externally.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#what-the-core-deliberately-leaves-out) — "What the core deliberately leaves out".

### 5.2 Lambdas do not capture
Lambdas (and blocks used as values) **MUST NOT** capture outer variables. All dependencies must be passed explicitly. This keeps effect tracking and the value-receiver check (§4.2) tractable.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#safety-the-compiler-proves-from-signatures-not-locks) — "Safety the compiler proves from signatures, not locks".

### 5.3 No `async`/`await` syntax
Zane does not define `async` or `await`. Concurrency is expressed only through `spawn` and compiler-managed parallelism. This avoids function coloring: a verb does not become a different kind of thing merely because some caller chooses to run it concurrently.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#parallelism-you-cant-see-concurrency-you-must-ask-for) — "Parallelism you can't see, concurrency you must ask for".

### 5.4 No language-level process or channel abstraction
Zane does not define a dedicated `Process` type, actor primitive, or channel primitive in the core language. Long-running concurrent work is expressed as ordinary spawned function or method calls plus explicit state flow governed by ownership and effect rules.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#what-the-core-deliberately-leaves-out) — "What the core deliberately leaves out".

---

## 6. Summary

| Concept | Rule |
|---|---|
| Implicit parallelism | Compiler may parallelize only when results are unchanged |
| `spawn` | Starts a concurrent function or method call; blocks only when results are read |
| Abortable `spawn` | Must attach `?` or `??` directly to the spawn expression |
| Water tower | A scope exits only after all spawned work completes |
| Mutation | A spawned mutating call requires a value-typed receiver; at most one mutable borrow per storage location; concurrent reads take a coherent snapshot |
