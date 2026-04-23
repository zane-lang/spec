# Zane Error Handling

This document specifies Zane's error handling model: how functions declare failure paths, how callers handle them, and the compile-time guarantees the system provides.

> **See also:** [`rationale.md`](rationale.md) §4 for the reasoning behind each design decision. [`comparison.md`](comparison.md) §2 for a comparison against C, Go, Java, Python, Rust, Swift, and Zig.

---

## 1. Overview

Zane implements a **Bifurcated Return Path** model for error handling. Every function has two distinct, type-safe return paths:

- **The Primary Path:** The normal, successful return. Declared as the left-hand return type.
- **The Secondary Path (Abort Path):** The failure return. Declared as the right-hand abort type, separated by `?`.

Both paths are **first-class types**, enforced at compile time. There are no implicit nulls, no hidden exceptions, and no unchecked error states. All error handling is **zero-cost** — implemented as conditional jumps at the call site, not as stack unwinding or heap-allocated union types.

---

## 2. Core Concepts

### 2.1 The Abort Type

A function that can fail declares an **abort type** as part of its return signature using the `?` separator:

```
ReturnType ? AbortType
```

- **`ReturnType`** is the type returned via `return` on the primary path.
- **`AbortType`** is the type returned via `abort` on the secondary path.

A function with **no `?` in its signature cannot abort**. The compiler guarantees this statically.

### 2.2 The `Void` Abort Type

If a function can fail but the failure carries no meaningful information, the abort type is `Void`. This mirrors the use of `Void` as a primary return type (the function completes but produces no value).

```zane
// Primary Void: completes, produces nothing
Void log(msg String) { ... }

// Secondary Void: fails, carries no reason
Int ? Void tryParse(input String) { ... }
```

When a function's abort type is `Void`, the `abort` keyword is used with no argument:

```zane
Int ? Void tryParse(input String) {
    if (input:isEmpty()) abort
    return input:toInt()
}
```

### 2.3 Effects (`mut`) vs. Abortability

`mut` and `?` are independent, orthogonal concepts:

- **`mut`** is a behavioral modifier on methods. It grants write access to `this`. A non-`mut` method can be passed where a `mut`-accepting function type is expected (behavioral upcasting is safe — a read-only function is a valid substitute where mutation is allowed but not required).
- **`? AbortType`** is a structural type. It changes the physical call contract between the function and its caller. A function's abort type **cannot** be changed implicitly. It is part of the function's type identity.

```zane
package Math

// non-mut can be assigned to a non-mut function variable
Int square(x Int) { return x * x }
fn (Int) -> Int = Math$square    // OK

// abort type cannot be dropped implicitly
Int ? String parse(s String) { ... }
fn2 (String) -> Int = Math$parse  // COMPILER ERROR: abort type 'String' is lost
```

---

## 3. Call Site Handling

When calling a function that can abort, the caller **must** handle the abort path. There are three mechanisms for this.

### 5.1 The `?` Handler Block

The primary mechanism. A block is attached to the call expression. The block receives the aborted value as a named binder (or no binder if the abort type is `Void`). Every path through the block must either:

- **`resolve Expression`** — Produce a value of `ReturnType`, substituting it as the result of the call expression.
- **`return Expression`** — Exit the entire parent function via its primary path.
- **`abort Expression`** — Exit the entire parent function via its secondary path (only valid if the parent function declares a compatible abort type).

```zane
package Main
import Parser
import Log

// With a named binder (AbortType is String)
Void example1(log Log) mut {
    x Int = parse("abc") ? err {
        log:write("Failed: " + err)
        resolve Int(0)
    }
}

// With no binder (AbortType is Void)
Void example2(fs FileSystem) {
    x Int = fs:tryRead("file.txt") ? {
        resolve Int(0)
    }
}

// Propagating the abort upward (parent must declare ? String)
Int ? String process(input String) {
    x Int = parse(input) ? err { abort err }
    return x * Int(2)
}

// Mixed: recover some errors, propagate others
Int ? Codes load(fs FileSystem, fileName String) {
    content String = fs:read(fileName) ? err {
        if (err == Codes$FileNotFound) { resolve "default" }
        abort err
    }
    return content:toInt()
}
```

### 5.2 Exhaustiveness Rule

The compiler performs **exhaustiveness checking** on every `?` handler block. If any code path through the block fails to `resolve`, `return`, or `abort`, it is a **compile-time error**.

```zane
// COMPILER ERROR: not all paths resolve/return/abort
x Int = parse("abc") ? err {
    log:write(err)
    // Missing resolve/return/abort!
}
```

### 5.3 The `??` Shorthand (Value Coalescing)

For the common case of "resolve with a default value if aborted", the `??` operator provides a concise inline shorthand. It is syntactic sugar for a `?` block containing only `resolve`:

```zane
// Sugar
x Int = parse("abc") ?? Int(0)

// Equivalent to
x Int = parse("abc") ? _ { resolve Int(0) }
```

`??` is valid regardless of the abort type, including `Void`.

### 5.4 Omitting the Result (`Void` Primary Return)

Just as a call to a `Void`-returning function must not be assigned to a variable, a call whose abort path is handled must not assign the abort binder when the abort type is `Void`:

```zane
// Primary Void: result is not assigned
log:write("hello")
s String = log:write("hello")    // COMPILER ERROR

// Secondary Void: binder is omitted in the handler
x Int = fs:tryRead("file.txt") ? {
    resolve Int(0)
}
```

---

## 4. The `resolve` Keyword

`resolve` is a **block-level return** that substitutes a value as the result of the aborted call expression. It does **not** exit the parent function. It is only valid inside a `?` handler block.

| Keyword   | Exits           | Valid In               | Purpose                                       |
|-----------|-----------------|------------------------|-----------------------------------------------|
| `resolve` | Handler block   | `?` handler only       | Substitute a success value                    |
| `return`  | Parent function | Anywhere               | Exit parent via primary path                  |
| `abort`   | Parent function | Anywhere (if declared) | Exit parent via secondary path                |

```zane
package Feature
import FileSystem

String ? Codes process(this Feature, fileName String) mut {
    // resolve: only exits the handler, process() continues
    content String = this.fs:read(fileName) ? err {
        resolve "default"
    }

    // return: exits process() entirely via primary path
    backup String = this.fs:read("backup.txt") ? err {
        return "hardcoded fallback"
    }

    // abort: exits process() entirely via secondary path
    final String = this.fs:read("final.txt") ? err {
        abort Codes$Unrecoverable
    }

    return content + backup + final
}
```

---

## 5. Connection to Zane's Effect Model

### 5.1 Abortability is orthogonal to `mut`

A method can be `mut` and aborting, `mut` and non-aborting, non-`mut` and aborting, or non-`mut` and non-aborting. All four combinations are valid. Neither property implies or restricts the other.

```zane
package Graph

// non-mut, non-aborting
Int scaledId(this Node, factor Int) {
    return this._id * factor
}

// non-mut, aborting
Int ? Codes safeScaledId(this Node, factor Int) {
    if (factor == Int(0)) abort Codes$ZeroFactor
    return this._id * factor
}

// mut, non-aborting
Void setScale(this Node, s Float) mut {
    this.scale = s
}

// mut, aborting
Void ? Codes setScaleSafe(this Node, s Float) mut {
    if (s < Float(0)) abort Codes$NegativeScale
    this.scale = s
}
```

The abort type is part of the structural function type and travels with method references:

```zane
Graph$safeScaledId    // type: (Graph$Node, Int) -> Int ? Codes
```

---

## 6. Compiler Guarantees

| Guarantee | Description |
|---|---|
| **No Unhandled Aborts** | Every call to an abortable function must have a `?` handler or `??`. Unhandled aborts are compile-time errors. |
| **Exhaustiveness** | Every path through a `?` handler block must `resolve`, `return`, or `abort`. |
| **Type Safety** | The `resolve` expression must match the `ReturnType` of the aborted call. The `abort` expression must match the `AbortType` of the parent function. |
| **Zero Runtime Overhead** | When handled immediately at the call site, aborts are compiled to a conditional jump. No heap allocation, no stack unwinding, no union storage. |
| **No Implicit Abort Type Dropping** | A function's abort type is part of its structural type. It cannot be silently dropped when assigning to a function variable. |
| **Abort-Free Guarantee** | A function with no `?` in its signature is **statically guaranteed** to never abort. The compiler verifies this transitively. |

---

## 7. Syntax Quick-Reference

> See [`syntax.md`](syntax.md) §3 for function and method declaration grammar, §2.4 for function type syntax, and §5 for the complete error handling syntax quick-reference.

