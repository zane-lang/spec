# Zane Purity Model

## 1. Overview

Zane’s purity model is a **two-layer system**:

1. **Structural inference (always on):** the compiler infers how “pure” each function is by analyzing:
   - what state it can reach through ownership and `ref`
   - which capability objects (I/O, system state) it can reach
   - what it calls (call graph propagation)

2. **Declared constraints (optional, enforced):** the programmer can declare **requirements** such as `pure` or `readonly` on a function. The compiler treats these as *contracts*: if the function body violates the constraint, compilation fails. This prevents silent purity regressions during refactors.

This is not an effect-tagging system: programmers do not list “effects”. Purity mostly falls out of how dependencies are wired and stored.

---

## 2. Package instances and “top-level is instance-member”

Zane packages are *instantiable*. Conceptually:

- `package Math` defines a **package type** `Math`.
- Top-level fields/functions declared inside `package Math` are members of that package type (the “package instance”).
- `import Math` imports the **type/API**, not a global singleton.
- `Math math = Math{ ... }` (or equivalent) constructs an instance.

This means there is no need to explicitly write `class Math { ... }`—the package body *is* the instance member set.

Example shape:

```zane
package Math
import Log

Log log

Math(Log log) {             // constructor for the package instance (syntax illustrative)
    this.log = log
}

Int square(Int x) pure {    // instance method; does not touch this => Total Pure
    return x * x
}

Void squareAndLog(Int x) {  // instance method; touches this.log => Full Impure
    log.write(x)
}
```

---

## 3. Definitions

### 3.1 Side effect
A **side effect** is any observable interaction beyond returning a value, including:

- Mutating `this` or owned descendants
- Reading through a `ref` (observing external mutable state)
- Writing through a `ref` (mutating external state)
- Performing I/O through capability objects
- Allocation/destruction of heap objects (allocator state)

### 3.2 Capability
A **capability** is any object whose methods represent interaction with external state/I/O (filesystem, console, network, logger, RNG, clock, etc.). Capabilities are ordinary objects; they become “reachable” only by being passed/stored.

---

## 4. The four inferred purity levels

The compiler classifies every function into one of four levels. Think of these as “how far effects reach”.

### Level 1 — Total Pure
**Definition:** depends only on explicit inputs and local computation. No reads/writes of external state; referentially transparent.

**Typical triggers to *lose* Total Pure:**
- reading/writing `this` state
- reading/writing through `ref`
- calling anything not Total Pure

**Example (Total Pure method inside a package):**
```zane
package Math

Int square(Int x) pure {
    return x * x
}
```

**Compiler freedoms:** constant folding, memoization, dead-code elimination, reordering, parallelization.

---

### Level 2 — Read-Only Impure
**Definition:** reads external state but does not mutate external state. Same inputs may produce different outputs because observed state can change.

**Example (reads through `ref`):**
```zane
package Hud
import Game

ref Game game

Hud(ref Game game) {
    this.game = game
}

Int currentScore() readonly {
    return game.score     // read through ref => Read-Only Impure (at most)
}
```

**Compiler limitations:** cannot cache across time; parallelization only if it can prove no relevant writer is concurrent.

---

### Level 3 — Package Pure (instance-local mutation)
**Definition:** mutates only the state owned by the current instance (`this` and its owned descendants). No writes through `ref`; no external I/O.

**Example (mutates only `this`):**
```zane
package Counter

Int n

Counter(Int start) {
    this.n = start
}

Void inc() {
    n = n + 1        // mutates only this package instance state => Package Pure
}

Int get() readonly {
    return n         // reading own state is still not Total Pure (it depends on history),
                     // but it's contained within this instance
}
```

**Concurrency:** safe to run concurrently on distinct instances; must serialize per-instance.

---

### Level 4 — Full Impure
**Definition:** effects escape the instance subtree or touch external systems. Includes:
- calling I/O capabilities
- writing through `ref`
- calling Full Impure functions
- allocation/destruction boundaries (see §9)

**Example (I/O capability reachable via stored field):**
```zane
package Log

// capability package (illustrative)
Void write(String msg) { /* writes to file/console */ }
```

```zane
package Math
import Log

Log log

Math(Log log) {
    this.log = log
}

Int square(Int x) pure {
    return x * x
}

Int squareAndLog(Int x) {
    log.write("x=" + x)   // capability call => Full Impure
    return x * x
}
```

**Compiler restrictions:** no elimination, caching, or reordering across relevant effects without explicit guarantees.

---

## 5. Structural inference rules (how the compiler decides)

Purity is inferred from:

1. **Reachability**
   - Owned fields: effects can be contained (Package Pure)
   - `ref` fields/params: effects can cross ownership subtrees (Read-only or Full Impure)
   - Capability fields/params: calling into them is Full Impure

2. **Operations**
   - write to owned state → at least Package Pure
   - read through `ref` → at least Read-Only Impure
   - write through `ref` → Full Impure
   - call to Full Impure → Full Impure

3. **Call-graph propagation**
   - a function is at most as pure as the least-pure operation/callee it uses.

**Conservative default:** if a call target’s purity is unknown (e.g., foreign object file without metadata), the compiler treats it as Full Impure.

---

## 6. Declared purity constraints (`pure`, `readonly`)

Inferred purity controls optimization; declared purity prevents regression and expresses intent in APIs.

### 6.1 `pure`
A function declared `pure` must satisfy **Total Pure**. The compiler rejects any external reads/writes or impure calls.

```zane
package Math
import Log

Log log

Int square(Int x) pure {
    // log.write(x)         // compile error: violates pure
    return x * x
}
```

### 6.2 `readonly`
A function declared `readonly` must not mutate external state. It may read external state.

```zane
package Hud
import Game

ref Game game

Int currentScore() readonly {
    return game.score       // OK (read-only external read)
}

Void cheat() readonly {
    // game.score += 1000    // compile error: external write via ref
}
```

### 6.3 Constraints are minimum guarantees
A `readonly` function is allowed to be inferred as Total Pure; it’s just “better than promised”. But it may not become Package Pure or Full Impure.

### 6.4 Purity in function types
Higher-order APIs can demand pure callbacks:

```zane
package Algo

// transform must be pure so Algo can safely parallelize/reorder
List<U> map(List<T> xs, (T) pure -> U transform) {
    ...
}
```

---

## 7. Using instantiated packages as values (and reducing prop drilling)

Because packages are instantiable, a package instance can be treated like any other value:
- passed as an argument
- stored as an owned field
- stored as a `ref` field

This is the primary “non-tag” mechanism for controlling effects: **if you don’t pass/store the capability instance, the code cannot perform that effect.**

### 7.1 Baseline: explicit passing everywhere (no prop drilling avoidance)
```zane
package Feature
import Log

Void doThing(Int x, Log log) {
    log.write("x=" + x)
}
```
This is maximally explicit but can cause prop drilling in deep call chains.

### 7.2 Store a capability instance in your package instance (constructor injection)
```zane
package Feature
import Log

Log log

Feature(Log log) { this.log = log }

Void doThing(Int x) {
    log.write("x=" + x)   // Full Impure
}
```
You pass `log` once at construction, not through every call.

### 7.3 Store a capability as a `ref` field (direct value reuse across layers)
This is the “fight prop drilling” pattern you asked to include: you can keep a long-lived capability owned elsewhere and store only a reference.

```zane
package Worker
import Log

ref Log log

Worker(ref Log log) {
    this.log = log
}

Void run() {
    log.write("running")  // Full Impure
}
```

**Purity impact:**
- any function that *uses* `log.write(...)` is Full Impure
- simply *reading* through `ref log` (e.g., checking a log level flag) is at least Read-Only Impure (`readonly` is the right constraint for “I promise I won’t write”)

**Lifetime impact:**
- `ref` does not extend lifetime; if the owner of `log` dies, the ref becomes null and dereference is a caught runtime error (per Zane’s memory model). This avoids U.B. but still requires sensible lifetime architecture (typically: capabilities owned by a long-lived root).

### 7.4 Context object pattern (reduce plumbing without globals)
Instead of storing many refs separately, bundle them:

```zane
package Context
import Log
import FileSystem
import Clock

Log fsLog
FileSystem fs
Clock clock

Context(Log fsLog, FileSystem fs, Clock clock) {
    this.fsLog = fsLog
    this.fs = fs
    this.clock = clock
}
```

```zane
package Feature
import Context

ref Context ctx

Feature(ref Context ctx) { this.ctx = ctx }

Void doThing() {
    ctx.fsLog.write("hi")         // Full Impure
    // ctx.clock.now()             // Read-Only Impure if clock is observational
}
```

This keeps dependency wiring explicit at the root, while avoiding long parameter lists.

---

## 8. Concurrency implications (summary)

- **Total Pure:** always safe to parallelize and reorder.
- **Read-Only Impure:** parallelize only if the compiler can prove no relevant writer is concurrent (otherwise requires explicit synchronization).
- **Package Pure:** parallelize across distinct instances; serialize per-instance.
- **Full Impure:** preserve ordering and avoid parallelization unless the programmer provides explicit synchronization/guarantees.

---

## 9. Constructors, destruction, allocation boundaries

Even if a method is Total Pure, **constructing and destroying objects** affects allocator state and deterministic destruction guarantees. For optimization purposes:

- constructors/destructors are treated as Full Impure boundaries
- allocation/destruction cannot be removed or reordered in ways that change observable lifetime/resource behavior

Example context:

```zane
package Main
import Math

Void main() {
    Math m = Math{ }        // allocation / construction boundary (effectful)
    Int y = m.square(3)     // Total Pure call (if square is pure)
}                           // destruction boundary (effectful)
```

This does not prevent pure computation; it just means “create/destroy” is effectful even if methods are pure.

---

## 10. Abort (`?`) is orthogonal to purity

Abortability is a control-flow contract, not an effect contract. A `pure` function may still abort.

```zane
package SafeMath

Int ? DivErr div(Int a, Int b) pure {
    if (b == 0) abort DivErr::ByZero
    return a / b
}
```

Purity and abortability compose independently.

---

## 11. Summary table

| Level | Reads external state | Writes external state | Cacheable | Removable if unused | Parallelizable |
|---|---:|---:|---:|---:|---:|
| Total Pure | No | No | Yes | Yes | Yes |
| Read-Only Impure | Yes | No | No | Sometimes (rare/provable) | With proof of no writer |
| Package Pure | Within `this` subtree | Within `this` subtree | No | No | Across distinct instances |
| Full Impure | Yes | Yes | No | No | Not without explicit sync |
