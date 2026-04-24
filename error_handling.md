# Zane Error Handling

This document specifies Zane's error handling model: how functions declare failure paths, how callers handle them, and the compile-time guarantees the system provides.

> **See also:** [`syntax.md`](syntax.md) §3 for function and method declaration grammar, §2.4 for function type syntax, and §5 for the complete error handling syntax quick-reference.

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

### 3.1 The `?` Handler Block

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

### 3.2 Exhaustiveness Rule

The compiler performs **exhaustiveness checking** on every `?` handler block. If any code path through the block fails to `resolve`, `return`, or `abort`, it is a **compile-time error**.

```zane
// COMPILER ERROR: not all paths resolve/return/abort
x Int = parse("abc") ? err {
    log:write(err)
    // Missing resolve/return/abort!
}
```

### 3.3 The `??` Shorthand (Value Coalescing)

For the common case of "resolve with a default value if aborted", the `??` operator provides a concise inline shorthand. It is syntactic sugar for a `?` block containing only `resolve`:

```zane
// Sugar
x Int = parse("abc") ?? Int(0)

// Equivalent to
x Int = parse("abc") ? _ { resolve Int(0) }
```

`??` is valid regardless of the abort type, including `Void`.

### 3.4 Omitting the Result (`Void` Primary Return)

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
Graph$safeScaledId    // type: (this Graph$Node, Int) -> Int ? Codes
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

## 7. Design Rationale

| Decision | Rationale |
|---|---|
| `?` separates return and abort types | Creates visual and syntactic symmetry between declaration and call site. The primary path is on the left; the secondary path is on the right. |
| Abort type is structural, not behavioral | Changing the abort type changes the physical call contract between function and caller. It cannot be dropped implicitly — it must be handled or explicitly propagated. |
| `mut` is behavioral, not structural | A `mut` method is a strict superset of a non-`mut` method in terms of behavior. The abort type is independent of `mut`. |
| `resolve` is a distinct keyword | Prevents ambiguity between "exit this block with a value" and "exit this function with a value." `return` and `abort` always refer to the parent function. `resolve` always refers to the handler block. Every other language leaves this to convention or labeled blocks. |
| `Void` abort type instead of empty `?` | Acknowledges that failure is a real, explicit code path. Mirrors the meaning of `Void` as a primary return type. Makes it impossible to accidentally omit handling — the abort type is always visible in the signature. |
| No stored `T?E` values | Abortability is a control flow construct, not a data construct. No `Result`-like value is created unless stored explicitly as `Union<T,E>`. This means zero heap allocation and zero union storage at the call site — just a conditional jump. |
| No implicit default initialization | Variables must be consciously set. The compiler uses Control Flow Graph analysis to guarantee all paths initialize a variable before use, with zero runtime overhead. |
| Exhaustiveness checking on every `?` handler | Every path through a `?` handler block must `resolve`, `return`, or `abort`. Missing paths are compile-time errors. This makes it impossible to silently fall through without handling the abort. |

---

## 8. Language Comparisons

### 8.1 Feature Matrix

| Feature | Zane | C | Go | Java | Python | Rust | Swift | Zig |
|---|---|---|---|---|---|---|---|---|
| Unhandled errors are compile-time errors | ✅ | ❌ | ❌ | ⚠️ Checked only | ❌ | ✅ | ✅ | ✅ |
| Error type is part of function signature | ✅ | ❌ | ❌ | ⚠️ Checked only | ❌ | ✅ | ❌ | ✅ |
| Zero-cost (no stack unwinding) | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ | ✅ |
| No union storage required at call site | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ |
| Inline recovery at call site | ✅ | ❌ | ❌ | ❌ | ❌ | ⚠️ Verbose | ⚠️ Verbose | ✅ |
| Error-free functions statically guaranteed | ✅ | ❌ | ❌ | ⚠️ Checked only | ❌ | ✅ | ❌ | ✅ |
| No null values | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| Coalescing shorthand (`??`) | ✅ | ❌ | ❌ | ❌ | ❌ | ⚠️ `.unwrap_or()` | ✅ | ❌ |
| Inline default value | ✅ | ❌ | ❌ | ❌ | ❌ | ⚠️ Verbose | ✅ | ✅ |
| Single effect annotation (`mut`) | ✅ | ❌ | ❌ | ❌ | ❌ | ⚠️ `&mut` | ❌ | ❌ |

### 8.2 Zane vs. C (Return Codes)

C signals failure by returning a sentinel value (usually `-1` or `NULL`) and setting a global `errno` variable.

**C:**
```c
FILE *f = fopen("file.txt", "r");
if (f == NULL) {
    fprintf(stderr, "Error: %d\n", errno);
    return -1;
}
```

**Zane:**
```zane
f File = fs:openFile("file.txt") ? err {
    log:write("Error: " + err)
    abort err
}
```

| Problem in C | How Zane Solves It |
|---|---|
| The compiler does not enforce that you check the return value. You can silently ignore `NULL`. | Every abortable call **must** have a `?` handler. Unhandled aborts are compile-time errors. |
| The error type (`errno`) is a global variable, not part of the function signature. | The abort type is part of the function signature (`Int ? Codes`). |
| Error codes and success values share the same return channel, requiring magic sentinel values. | The primary and secondary paths are completely separate. |
| Functions cannot be composed without manual intermediate checks. | `??` and inline `?` blocks allow safe composition in a single expression. |

### 8.3 Zane vs. Go (Multiple Return Values)

**Go:**
```go
content, err := os.ReadFile("file.txt")
if err != nil {
    return "", err
}
```

**Zane:**
```zane
content String = fs:readFile("file.txt") ? err { abort err }
```

| Problem in Go | How Zane Solves It |
|---|---|
| The compiler does not enforce that you check the `error` return. | Every abortable call **must** have a `?` handler. |
| The `if err != nil` pattern creates significant boilerplate. | The `?` handler and `??` shorthand replace it with concise inline constructs. |
| Error propagation requires manually constructing zero values for other return values. | `abort err` propagates cleanly with no dummy values. |
| The `error` type is a generic interface with no compile-time distinction between error kinds. | The abort type is fully typed (`? IOError`, `? ParseError`). |

### 8.4 Zane vs. Java (Checked Exceptions)

**Java:**
```java
try {
    String content = readFile("file.txt");
} catch (IOException e) {
    System.out.println("Failed: " + e.getMessage());
    content = "default";
}
```

**Zane:**
```zane
content String = fs:readFile("file.txt") ? err {
    log:write("Failed: " + err)
    resolve "default"
}
```

| Problem in Java | How Zane Solves It |
|---|---|
| `try/catch` blocks are physically separated from the call site. | The `?` handler is always **adjacent to the call**. |
| Exceptions use **stack unwinding**, which is expensive. | Zane uses zero-cost **conditional jumps**. |
| Unchecked exceptions (`RuntimeException`) escape the type system entirely. | All abort paths are declared and enforced by the compiler. |
| Inline recovery is not possible. | `??` and `?` handler enable inline, expression-level recovery. |

### 8.5 Zane vs. Python (Unchecked Exceptions)

**Python:**
```python
try:
    content = read_file("file.txt")
except FileNotFoundError as e:
    content = "default"
```

**Zane:**
```zane
content String = fs:readFile("file.txt") ? err {
    resolve "default"
}
```

| Problem in Python | How Zane Solves It |
|---|---|
| The type system has no knowledge of what exceptions a function raises. | The abort type is part of the function signature. |
| Any function can raise any exception with no static guarantee. | Functions without `?` are statically guaranteed to never abort. |
| Exceptions unwind the stack. | Zane uses zero-cost conditional jumps. |

### 8.6 Zane vs. Rust (`Result<T, E>`)

**Rust:**
```rust
let content = read_file("file.txt").unwrap_or_else(|e| {
    eprintln!("Failed: {}", e);
    "default".to_string()
});
```

**Zane:**
```zane
content String = fs:readFile("file.txt") ? err {
    log:write("Failed: " + err)
    resolve "default"
}
```

| Difference | Rust | Zane |
|---|---|---|
| **Error representation** | `Result<T, E>` is a real enum stored in a register or on the stack. | The abort path is a **control flow construct**. No `Result` value is created unless stored explicitly as `Union<T,E>`. |
| **Inline recovery** | `.unwrap_or()`, `.unwrap_or_else()`, `.map_err()` — functional but verbose. | `?` handler block and `??` — block-scoped, readable, supports multi-line logic. |
| **Propagation** | `?` operator propagates automatically. | `? err { abort err }` is explicit. |
| **Storing errors** | `Result<T, E>` can be stored and passed as a value. | Storing requires explicit `Union<T, E>`. |
| **Effect system** | `&self` vs `&mut self` on references. | `mut` on method declarations; receiver-only mutation. |

### 8.7 Zane vs. Swift (`throws`)

**Swift:**
```swift
let content: String
do {
    content = try readFile("file.txt")
} catch {
    content = "default"
}
```

**Zane:**
```zane
content String = fs:readFile("file.txt") ? err {
    resolve "default"
}
```

| Difference | Swift | Zane |
|---|---|---|
| **Error type in signature** | `throws` does not declare the error type. | The abort type is fully typed (`? FileError`). |
| **Call site structure** | `do { try ... } catch { ... }` requires pre-declared variable. | `?` handler is inline; result available immediately. |
| **Stack unwinding** | Swift exceptions use stack unwinding. | Zane uses zero-cost conditional jumps. |
| **`??` coalescing** | `try?` silently discards the error type. | `??` coalesces cleanly; abort type still known to compiler. |

### 8.8 Zane vs. Zig (Error Unions)

**Zig:**
```zig
const content = readFile("file.txt") catch |err| blk: {
    std.debug.print("Failed: {}\n", .{err});
    break :blk "default";
};
```

**Zane:**
```zane
content String = fs:readFile("file.txt") ? err {
    log:write("Failed: " + err)
    resolve "default"
}
```

| Difference | Zig | Zane |
|---|---|---|
| **Type order** | `ErrorType!ReturnType` — error first. | `ReturnType ? AbortType` — return first (C-family convention). |
| **Block labels** | `catch \|err\| blk: { break :blk value }` — labeled block + break. | `? err { resolve value }` — dedicated `resolve` keyword, no labels. |
| **Void abort type** | Zig infers error sets automatically. | Zane requires explicit `Void`. |
| **Effect system** | None. | `mut` as single effect marker. |

### 8.9 Summary

Zane sits at a unique intersection in the error handling design space. It combines:

- **The static enforcement of Rust and Zig** — unhandled aborts are compile-time errors.
- **The call-site ergonomics of Swift** — inline, expression-oriented recovery without pre-declaring variables.
- **The zero-cost model of C and Zig** — conditional jumps, no stack unwinding, no forced union storage.
- **The typed error contracts of Rust and Zig** — the abort type is part of the function's structural type.
- **A single effect marker** — `mut` on methods is the only user-facing annotation; no `pure`, `readonly`, or effect lists.
- **A unique innovation** — the `resolve` keyword, which cleanly separates "substitute a value here" from "exit this function," an ambiguity that every other language leaves to convention or labeled blocks.

---

## 9. Syntax Quick-Reference

> See [`syntax.md`](syntax.md) §3 for function and method declaration grammar, §2.4 for function type syntax, and §5 for the complete error handling syntax quick-reference.
