# Zane Purity / Effects Model

## 1. Overview

Zane uses a **structural effect model** with a single user-facing effect modifier: `mut`.

The compiler infers how effectful each function is by analyzing:

- what state it can reach through ownership and `ref`
- which capability objects (I/O, system state) it can reach
- whether it is a method on `this`
- what other functions it calls
- whether it is marked `mut`

There are **no user-facing purity tags** like `pure` or `readonly`.  
Instead:

- functions and methods are **read-only by default**
- methods marked `mut` may mutate their receiver (`this`)
- the compiler derives all stronger properties automatically

This keeps the language simple for programmers while still giving the compiler the information it needs for optimization and concurrency analysis.

> **See also:** [`oop.md`](oop.md) for package structure, class/struct declarations, constructor syntax, method declarations, and overloading rules. This document covers only the effect-inference layer that sits on top of those constructs.

---

## 2. Core Definitions

### 2.1 Side effect

A **side effect** is any observable interaction beyond returning a value, including:

- writing to `this` or owned descendants
- reading through a `ref`
- writing through a `ref`
- performing I/O through a capability object
- allocating or destroying heap objects

### 2.2 Capability

A **capability** is any object whose methods represent access to external state or I/O:

- filesystem
- console
- logger
- network
- clock
- random generator
- FFI bridge objects

Capabilities are ordinary objects. Code can only use them if they are explicitly passed or stored.

### 2.3 `mut`

`mut` is the only effect modifier in Zane.

A method marked `mut` may:

- write to `this`
- write to objects owned by `this`
- call other `mut` methods on `this` or owned fields

A method without `mut` may:

- read `this`
- read its parameters
- call only non-`mut` functions/methods

A function may never mutate its explicit parameters directly.

---

## 3. Inferred Effect Levels

The compiler internally classifies each function into one of four levels. These are **not syntax**. They are semantic categories used for optimization and concurrency reasoning.

---

### Level 1 — Total Pure

**Definition:** depends only on explicit parameters and immutable package constants. No reads or writes of mutable external state.

Examples:

```zane
package Math

Int square(x Int) {
    return x * x
}

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}
```

Both are Total Pure.

**Compiler freedoms:**
- constant folding
- memoization
- dead code elimination
- reordering
- parallelization

---

### Level 2 — Read-Only Impure

**Definition:** reads mutable external state but does not mutate it.

This includes:
- reading through a `ref`
- reading from observational capabilities
- reading object state that may vary between calls

Example:

```zane
package Hud
import Game

class Hud {
    game ref Game
}

Hud(game ref Game) {
    return init{
        game: game
    }
}

Int currentScore(this Hud) {
    return this.game.score
}
```

`currentScore` is Read-Only Impure because it reads through `ref`.

**Compiler limitations:**
- cannot globally memoize across time
- cannot freely reorder past writes to the same state
- parallelization requires proof that no writer is concurrent

---

### Level 3 — Instance-Local Mutation

**Definition:** mutates only `this` and its owned subtree. No writes through `ref`. No external I/O.

Example:

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
    this.n = this.n + Int(1)
}
```

`inc` mutates only the receiver, so it is instance-local.

**Concurrency:**
- safe across distinct instances
- must serialize on the same instance

---

### Level 4 — Full Impure

**Definition:** effects escape the receiver's ownership subtree or touch external systems.

This includes:
- calling `mut` methods on capabilities
- writing through a `ref`
- allocation/destruction boundaries
- calling other Full Impure functions

Example:

```zane
package Log

class Log {
    Void write(this Log, msg String) mut { ... }
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

Void run(this Feature) mut {
    this.log:write("hello")
}
```

`run` is Full Impure because it performs I/O through a capability.

---

## 4. Effect Enforcement

### 4.1 Non-`mut` methods cannot call `mut` methods

```zane
package Feature
import Log

class Feature {
    log Log
}

Void bad(this Feature) {
    this.log:write("hi")    // compile error
}

Void good(this Feature) mut {
    this.log:write("hi")
}
```

This prevents hidden mutation from read-only contexts.

---

## 5. Structural Inference

### 5.1 Ownership layout analysis

The compiler uses type layout to infer reachable state:

- owned fields → mutation/read may stay local
- `ref` fields → effects may cross ownership subtrees
- capability fields → may cause external effects

### 5.2 Call graph propagation

A function is at most as "pure" as the least-pure thing it does or calls.

```zane
package Math

Int add(a Int, b Int) {
    return a + b
}

Int doubleAdd(x Int) {
    return add(x, x)
}
```

If `add` is Total Pure, `doubleAdd` can also be Total Pure.

### 5.3 Unknown callees are assumed Full Impure

If the compiler cannot prove a callee's behavior, it assumes the worst.

---

## 6. Capability Wiring and Prop Drilling

Because there is no ambient authority, effects only become possible if capabilities are explicitly passed or stored.

### 6.1 Explicit parameter passing

```zane
package Feature
import Log

Void doThing(x Int, log Log) mut {
    log:write("x")
}
```

### 6.2 Constructor injection

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
    this.log:write("x")
}
```

### 6.3 `ref` field injection

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

### 6.4 Context object pattern

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
    this.ctx.log:write("hi")
}
```

---

## 7. Constructors, Allocation, and Destruction

### 7.1 Constructors are not methods

Constructors have no `this`. They produce a new instance with `init{ }`. See [`oop.md`](oop.md) §3 for full constructor syntax and semantics.

### 7.2 Allocation/destruction are effect boundaries

Even if a constructor body is logically simple, object creation and destruction still affect allocator state.

```zane
package Main
import Graph

Void main() {
    node Graph$Node(Int(1))      // allocation boundary
    // ...
}                                // destruction boundary
```

For optimization purposes, construction and destruction are treated as Full Impure boundaries.

---

## 8. Abortability is Orthogonal

Abortability (`?`) is independent of effects. A function can be Total Pure and still abort.

```zane
package SafeMath

Int ? DivErr div(a Int, b Int) {
    if (b == Int(0)) abort DivErr$ByZero
    return a / b
}
```

The compiler may still optimize this aggressively if it can prove the abort path is unreachable.

> **See also:** [`error_handling.md`](error_handling.md) for full abortability semantics.

---

## 9. Concurrency Implications

| Level | Concurrency rule |
|---|---|
| Total Pure | always safe to reorder and parallelize |
| Read-Only Impure | safe only with proof of no concurrent writer |
| Instance-Local Mutation | safe across distinct instances, not same instance |
| Full Impure | preserve order unless explicitly synchronized |

Because Zane has single ownership, the compiler can often prove that two instance-local `mut` calls act on disjoint subtrees.

---

## 10. Summary Table

| Level | Reads external state | Writes external state | Cacheable | Removable if unused | Parallelizable |
|---|---:|---:|---:|---:|---:|
| Total Pure | No | No | Yes | Yes | Yes |
| Read-Only Impure | Yes | No | No | Sometimes | With proof of no writer |
| Instance-Local Mutation | Only within `this` subtree | Only within `this` subtree | No | No | Across distinct instances |
| Full Impure | Yes | Yes | No | No | Not without explicit sync |

---

## 11. Design Principle

Zane's effect model is based on one rule:

> **The only way code can mutate state is through a `mut` method on a receiver.**

Everything else follows from structure:

- ownership tells the compiler what can be reached
- `ref` tells it where reads/writes can escape
- capabilities tell it where external effects live
- `mut` tells it where mutation is allowed

There is no `pure` keyword. The compiler still infers when code is Total Pure, but that is an internal property used for optimization, not a user annotation.
