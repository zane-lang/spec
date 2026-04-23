# Zane Purity / Effects Model — Specification

This document specifies Zane's purity and effect model. It reflects the refined OOP design where:

- packages are **namespaces**, not classes
- a package may optionally define a **class of the same name** for instanceful behavior
- methods are **free functions** with `this` as their first parameter
- `mut` is the **only effect marker** (replaces the earlier `pure`/`readonly` declared constraint pair for most cases)
- `pure` survives as an optional **declared constraint** for total referential transparency

Syntax is illustrative and subject to change; semantics are normative.

---

## 1. Overview

Zane's purity model is a **two-layer system**:

1. **Structural inference (always on):** the compiler classifies every function's effect level by analyzing the ownership tree, `ref` edges, capability reachability, and the call graph. No annotations required for the compiler to reason about effects.

2. **Declared constraints (optional, enforced):** the programmer may declare `pure` on a function to assert total referential transparency. The compiler verifies this and rejects any violation at compile time, preventing silent purity regressions.

This is not an effect-tagging system. Programmers do not maintain lists of effects. Effect classification falls out of how data and capabilities are wired through the ownership tree, and how `mut` is used on methods.

---

## 2. Packages, Classes, and the Effect Boundary

### 2.1 Packages are namespaces

A `package X` is a namespace. It contains:
- type declarations (`class`, `struct`, etc.)
- immutable constants (package-level values)
- static free functions (no `this`, no instance required)

Package-level constants are immutable. Package-level static functions can only touch their explicit parameters and those constants, making them always at least Read-Only with respect to external state.

```zane
package Math

pi Float(3.141592)

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}
```

`Math$radsToDeg` is called without any instance and is referentially transparent.

### 2.2 The instanceful package pattern

If a package defines a class of the same name, it gains an instantiable stateful object alongside its static members. The class is an ordinary class — its fields define what state a "Math instance" owns, including any capabilities it needs.

```zane
package Math
import Log

pi Float(3.141592)

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}

class Math {
    log Log
}

Math(log Log) {
    return init{
        log: log
    }
}

Void debugPi(this Math) mut {
    this.log:write(Math$radsToDeg(pi))
}
```

```zane
package Main
import Math
import Log

Void main() {
    log Log("stdout")

    deg Float = Math$radsToDeg(Float(1))    // static — no instance
    math Math(log)
    math:debugPi()                          // instance method — Full Impure
}
```

### 2.3 Scope isolation enforces the effect boundary

Constructor and method bodies cannot access package-level values directly. Any state a constructor or method depends on must arrive as an explicit parameter. This prevents hidden ambient dependencies and keeps the ownership/capability graph explicit and analyzable.

```zane
package Graph

counter Int = Int(0)

class Node {
    _id Int
    scale Float
}

Node(id Int) {
    return init{
        _id: id,
        scale: Float(1)
        // counter not accessible here — compile error if referenced
    }
}
```

---

## 3. Definitions

### 3.1 Side effect

A **side effect** is any observable interaction beyond returning a value, including:

- Writing to `this` or any owned descendant (via a `mut` method)
- Reading through a `ref` (observing external mutable state)
- Writing through a `ref` (mutating external state)
- Performing I/O through a capability object
- Allocation or destruction of heap objects (allocator state is affected)

### 3.2 Capability

A **capability** is any object whose `mut` methods represent interaction with external state or I/O (filesystem, console, network, logger, RNG, clock, etc.). Capabilities are ordinary class instances. A function gains access to a capability only by receiving or storing an instance explicitly — there is no ambient authority.

### 3.3 `mut` as the effect marker

`mut` is the only user-facing effect annotation in Zane. It is a modifier on a method declaration that grants write access to `this`. It does not appear on free functions (which have no `this`) or constructors (which produce `this` from nothing via `init{ }`).

The rule is simple:

> The only way any code can mutate state is by calling a `mut` method on some receiver. Parameters are always read-only.

This makes every effect traceable to a specific `mut` call on a specific receiver.

---

## 4. The Four Inferred Effect Levels

The compiler classifies every function into one of four levels based on structural analysis. These levels reflect "how far effects reach" relative to the function's position in the ownership tree.

---

### Level 1 — Total Pure

**Definition:** depends only on explicit parameters and local computation. Does not read or write any external state. Referentially transparent: the same inputs always produce the same output and the call can be replaced by its result.

**What causes loss of Total Pure:**
- reading `this` (makes result depend on instance state)
- reading through a `ref`
- calling anything not Total Pure

**Example:**
```zane
package Math

Int square(Int x) {
    return x * x
}

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}
```

Both are Total Pure: no `this`, no `ref`, no capability, no impure callees.

**Compiler freedoms:** constant folding, memoization, dead code elimination, reordering, parallelization, common subexpression elimination, loop hoisting.

---

### Level 2 — Read-Only Impure

**Definition:** reads external state (through a `ref` field, `ref` parameter, or read-only capability access) but does not mutate anything outside local scope. Same inputs may produce different outputs over time because observed state can change.

**Example:**
```zane
package Hud
import Game

class Hud {
    display ref Game
}

Hud(display ref Game) {
    return init{
        display: display
    }
}

Int currentScore(this Hud) {
    return this.display.score    // reads through ref => Read-Only Impure
}
```

**Compiler limitations:** cannot cache across time; can only reorder or parallelize with proof that no relevant writer runs concurrently.

---

### Level 3 — Package Pure (instance-local mutation)

**Definition:** mutates only state owned by `this` and its owned descendants. Does not write through any `ref`. Does not call capability methods that perform external I/O.

**Example:**
```zane
package Counter

class Counter {
    n Int
}

Counter(start Int) {
    return init{
        n: start
    }
}

Void inc(this Counter) mut {
    this.n = this.n + Int(1)    // mutates only this => Package Pure
}

Int get(this Counter) {
    return this.n
}
```

**Concurrency:** safe to parallelize across distinct instances (non-overlapping ownership subtrees); must serialize calls on the same instance.

---

### Level 4 — Full Impure

**Definition:** effects escape the instance's ownership subtree or touch external systems. Triggers include:
- calling a `mut` method on a capability (I/O)
- writing through a `ref`
- calling any Full Impure function
- allocation/destruction boundaries

**Example:**
```zane
package Log

class Log {
    Void write(this Log, msg String) mut { ... }    // FFI bridge: effectful
}
```

```zane
package Feature
import Log

class Feature {
    log Log
}

Feature(log Log) {
    return init{
        log: log
    }
}

Void doThing(this Feature, x Int) mut {
    this.log:write("x=" + x)    // calls mut on capability => Full Impure
}
```

**Compiler restrictions:** no elimination, caching, or reordering across relevant effects.

---

## 5. How `mut` Maps onto the Four Levels

With `mut` as the only annotation, the compiler can derive the effect level of any method mechanically:

| Method form | What it can do | Inferred level |
|---|---|---|
| No `this` (free/static function) | touches only params and package constants | Total Pure |
| `this`, no `mut`, no `ref` fields, no capability fields | reads `this` and params | Level 2 or Total Pure depending on callees |
| `this`, no `mut`, has `ref` fields | reads through `ref` | Read-Only Impure |
| `this`, `mut`, no capability in subtree | writes `this` only | Package Pure |
| `this`, `mut`, capability reachable | writes `this`, calls I/O via capability | Full Impure |
| `this`, no `mut`, calls `mut` on capability | compile error: cannot call `mut` from non-`mut` context | — |

The last row is enforced by the compiler: a non-`mut` method cannot call `mut` methods on owned fields because that would mutate `this` indirectly.

```zane
package Feature
import Log

class Feature {
    log Log
}

Void bad(this Feature) {
    this.log:write("hi")    // compile error: calling mut method from non-mut context
}

Void good(this Feature) mut {
    this.log:write("hi")    // ok
}
```

---

## 6. Structural Inference

### 6.1 Ownership layout analysis

Because every class has a statically known set of owned fields and `ref` fields, the compiler can determine at class-definition time what categories of state are reachable from any instance. A class with no `ref` fields and no capability fields anywhere in its ownership subtree is structurally incapable of producing Full Impure or Read-Only Impure methods.

```zane
package Math

class Math {
    // no fields
}

// all methods on Math are at most Total Pure
// Math has no state to read or write
```

### 6.2 Call graph propagation

Purity propagates bottom-up through the call graph. A function's inferred level is the minimum of its own direct effects and the levels of all functions it calls. The least-pure callee dominates.

```zane
package Graph

Int add(a Int, b Int) {
    return a + b                // Total Pure
}

Int scaledAdd(this Node, b Int) {
    return add(this.scale, b)   // calls Total Pure + reads this => Read-Only or Package Pure
}
```

### 6.3 Conservative default for unknown callees

If a callee's effect level cannot be determined (e.g., FFI without metadata), the compiler assumes Full Impure. Pre-compiled Zane object files may carry effect metadata; when present, the compiler uses it.

---

## 7. Declared Purity Constraint: `pure`

### 7.1 What `pure` means

`pure` is an optional declared constraint asserting **Total Pure**: the function depends only on its explicit parameters, reads no external state, and produces no observable effects. The compiler verifies this and rejects any violation.

```zane
package Math

Int square(Int x) pure {
    return x * x
}
```

### 7.2 What `pure` is for

`pure` is not needed for the compiler to infer Total Pure and apply optimizations — inference handles that. `pure` exists to:

- **prevent regression**: once declared, no future change can silently degrade purity. Adding a `ref`, a capability field, or an impure callee becomes a compile-time error.
- **express intent**: distinguishes "accidentally pure right now" from "must remain pure forever".
- **enable API contracts**: higher-order functions that need referentially transparent callbacks can demand `pure` in the function type.

```zane
package Algo

// caller guarantees transform can be memoized/parallelized
List<U> map(List<T> xs, (T) pure -> U transform) {
    ...
}
```

### 7.3 `pure` is a minimum guarantee

A function that is declared `pure` but inferred at a higher purity level (i.e., it is even "more pure" than Total Pure — which is impossible, Total Pure is the maximum) is always fine. A function declared `pure` that is inferred below Total Pure is a compile-time error.

### 7.4 `mut` and `pure` are mutually exclusive

A `mut` method writes `this` by definition. Writing is a side effect. A method cannot be both `pure` and `mut`.

```zane
Void inc(this Counter) mut pure {    // compile error: mut and pure are contradictory
    this.n = this.n + Int(1)
}
```

---

## 8. Capability Wiring and Prop Drilling

Because there is no ambient authority, capabilities must be passed or stored explicitly. This section describes common patterns for doing so without excessive prop drilling.

### 8.1 Explicit parameter passing (most transparent)

```zane
package Feature
import Log

Void doThing(x Int, log Log) {
    log:write("x=" + x)
}
```

Maximally explicit. Every call site must supply `log`. Best for shallow call chains.

### 8.2 Constructor injection (owned capability)

```zane
package Feature
import Log

class Feature {
    log Log
}

Feature(log Log) {
    return init{
        log: log
    }
}

Void doThing(this Feature, x Int) mut {
    this.log:write("x=" + x)
}
```

`log` is passed once at construction. Methods that use it are Full Impure; methods that do not are unaffected.

### 8.3 `ref` field injection (non-owning capability reference)

```zane
package Worker
import Log

class Worker {
    log ref Log
}

Worker(log ref Log) {
    return init{
        log: log
    }
}

Void run(this Worker) mut {
    this.log:write("running")
}
```

The `Worker` does not own the logger. The logger's lifetime must outlive the `Worker` — if the owner of `log` is destroyed while `Worker` holds a `ref`, the ref becomes null and dereference is a caught runtime error (per the memory model spec).

**Effect impact:** reading through `ref log` (e.g., checking a log level) is at least Read-Only Impure. Calling `mut` methods on it is Full Impure.

### 8.4 Context object pattern

```zane
package Context
import Log
import FileSystem
import Clock

class Context {
    log Log
    fs FileSystem
    clock Clock
}

Context(log Log, fs FileSystem, clock Clock) {
    return init{
        log: log,
        fs: fs,
        clock: clock
    }
}
```

```zane
package Feature
import Context

class Feature {
    ctx ref Context
}

Feature(ctx ref Context) {
    return init{
        ctx: ctx
    }
}

Void doThing(this Feature) mut {
    this.ctx.log:write("hi")    // Full Impure
}
```

Bundles multiple capabilities into one object passed once. Keeps wiring explicit at the root while avoiding long parameter lists deeper in the tree.

---

## 9. Methods as Values and Effect Types

### 9.1 Method references via package namespace

Methods are free functions in the package namespace. They are referenced as values using `$`, like any other package-level function. When used as a value, `this` becomes an explicit first argument.

```zane
package Graph

Int scaledId(this Node, factor Int) {
    return this._id * factor
}

Void setScale(this Node, s Float) mut {
    this.scale = s
}
```

References:
```zane
Graph$scaledId    // type: (Graph$Node, Int) -> Int
Graph$setScale    // type: (mut Graph$Node, Float) -> Void
```

`mut` appears in the function type of `mut` method references, signaling to callers that invoking the reference may mutate the first argument.

### 9.2 Purity in function types

Higher-order functions may demand a specific effect level via the function type:

```zane
package Algo

// demands Total Pure: compiler may memoize/parallelize calls to transform
List<U> map(List<T> xs, (T) pure -> U transform) { ... }

// accepts any callable (no purity constraint)
Void forEach(List<T> xs, (T) -> Void action) { ... }

// accepts mut callable (may mutate first arg)
Void mutateAll(List<T> xs, (mut T) -> Void action) { ... }
```

---

## 10. Constructors and the Effect Model

### 10.1 Constructors produce objects; they do not mutate existing ones

A constructor has no `this`. It produces a new instance via `init{ }` and returns it. The concept of `mut` does not apply. The allocation itself is treated as Full Impure (it advances the heap frontier), but this is a property of the call site, not of the constructor body.

```zane
package Main
import Graph

Void main() {
    node Graph$Node(Int(1))     // allocation: Full Impure boundary
    result Int = node:scaledId(Int(5))  // Total Pure if scaledId has no side effects
}                               // destruction: Full Impure boundary
```

### 10.2 Constructor bodies are scope-isolated

Constructor bodies cannot access package-level values (see §2.3). Any state they need must be an explicit parameter. This makes construction deterministic and keeps the capability graph visible at the call site.

---

## 11. Abort Paths are Orthogonal to Purity

The abort mechanism (`?`) is a control-flow contract, not an effect contract. A `pure` function may abort. Aborting is a conditional jump — it allocates nothing, writes no external state, and is not observable beyond the function boundary except through the value it carries.

```zane
package SafeMath

Int ? DivErr div(Int a, Int b) pure {
    if (b == 0) abort DivErr::ByZero
    return a / b
}
```

The compiler may apply all Total Pure optimizations to a pure aborting function, including eliminating the abort path if it can prove the condition is unreachable for given inputs.

---

## 12. Concurrency Implications

| Level | Threading rule |
|---|---|
| Total Pure | Always safe to parallelize and reorder freely |
| Read-Only Impure | Safe to parallelize with proof that no writer is concurrent; otherwise requires explicit synchronization |
| Package Pure | Safe to parallelize across distinct instances; must serialize on same instance |
| Full Impure | Preserve source order; parallelize only with explicit synchronization |

The ownership tree provides the compiler's primary tool for proving non-overlap: two independently-owned instances cannot share mutable state, so Package Pure calls on them can always be parallelized.

---

## 13. Summary Table

| Level | Reads external state | Writes external state | Cacheable | Removable if unused | Parallelizable |
|---|---|---|---|---|---|
| Total Pure | No | No | Yes | Yes | Always |
| Read-Only Impure | Yes | No | No | Sometimes | With proof of no writer |
| Package Pure | Own subtree only | Own subtree only | No | No | Across distinct instances |
| Full Impure | Yes | Yes | No | No | Not without explicit sync |

---

## 14. Design Principle

The purity model is designed around one principle:

> **The compiler should be able to prove as much as possible from the structure of the program — ownership layout, `ref` edges, capability wiring, and `mut` annotations — without requiring the programmer to maintain effect lists.**

`mut` is the only required annotation and it carries a single, unambiguous meaning: this method may write to its receiver. Everything else — which level a function sits at, whether it is safe to cache or parallelize, whether it can be eliminated — follows from structure alone. `pure` exists only when the programmer wants to make a permanent, compiler-enforced promise that a function will always remain at Level 1.
