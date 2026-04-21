# Error Handling Specification
## Zane Programming Language

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

```cpp
// Primary Void: completes, produces nothing
Void log(String message) { ... }

// Secondary Void: fails, carries no reason
Int ? Void tryParse(String input) { ... }
```

When a function's abort type is `Void`, the `abort` keyword is used with no argument:

```cpp
Int ? Void tryParse(String input) pure {
    if (input.isEmpty()) abort       // Void abort: no payload
    return input.toInt()
}
```

### 2.3 Purity (`pure`) vs. Abortability
`pure` and `?` are independent, orthogonal concepts:

- **`pure`** is a behavioral modifier. It guarantees no side effects. A `pure` function can be silently assigned to a non-`pure` function variable (behavioral upcasting is safe).
- **`? AbortType`** is a structural type. It changes the physical call contract between the function and its caller. A function's abort type **cannot** be changed implicitly. It is part of the function's type identity.

```cpp
// pure can be upcast to non-pure (safe)
Int square(Int x) pure { return x * x }
Int(Int) fn = square    // OK: pure assigned to non-pure

// Abort type cannot be upcast or dropped implicitly
Int ? String parse(String s) pure { ... }
Int(String) fn2 = parse // COMPILER ERROR: abort type 'String' is lost
```

---

## 3. Function Declaration Syntax

### 3.1 Standard Block Syntax
```
ReturnType ? AbortType FunctionName(Parameters) [Modifiers] { Body }
```

```cpp
// Returns Int, aborts with a String message
Int ? String parse(String input) pure {
    if (input.isEmpty()) abort "Input was empty"
    return input.toInt()
}

// Returns Int, aborts with a typed error code
Int ? Codes divide(Int a, Int b) pure {
    if (b == 0) abort Codes::DivisionByZero
    return a / b
}

// Returns Int, aborts with no payload
Int ? Void tryRead(String fileName) {
    if (!fs::exists(fileName)) abort
    return fs::read(fileName)
}

// Cannot abort at all (no ? in signature)
Int square(Int x) pure {
    return x * x
}
```

### 3.2 Shorthand `=>` Syntax
For single-expression functions, the `=>` shorthand can be used. If the expression on the right side already handles all abort paths (e.g. via `??`), the function's signature requires no `?`:

```cpp
// Shorthand, can abort (passes abort upward)
Int ? String processInput(String s) pure => parse(s)

// Shorthand, cannot abort (abort is handled inline by ??)
Int safeParseOrZero(String s) pure => parse(s) ?? 0
```

---

## 4. Function Type Syntax

Function types follow the same structure, making higher-order functions fully type-safe:

```
(ParameterTypes) [Modifiers] -> ReturnType ? AbortType
```

```cpp
// A pure function taking an Int, returning Int, cannot abort
(Int) pure -> Int

// A pure function taking a String, returning Int, aborts with String
(String) pure -> Int ? String

// A non-pure function taking a String, returning Int, aborts with Void
(String) -> Int ? Void

// A pure function taking two Ints, returning Int, aborts with Codes
(Int, Int) pure -> Int ? Codes
```

**Example: Higher-order function using function types:**
```cpp
Array<Int> ? String parseAll(
    Array<String> inputs,
    (String) pure -> Int ? String parser
) pure {
    Array<Int> results = []
    for (str in inputs) {
        results.push(parser(str) ? err { abort err })
    }
    return results
}
```

---

## 5. Call Site Handling

When calling a function that can abort, the caller **must** handle the abort path. There are three mechanisms for this.

### 5.1 The `?` Handler Block
The primary mechanism. A block is attached to the call expression. The block receives the aborted value as a named binder (or no binder if the abort type is `Void`). Every path through the block must either:
- **`resolve Expression`** — Produce a value of `ReturnType`, substituting it as the result of the call expression.
- **`return Expression`** — Exit the entire parent function via its primary path.
- **`abort Expression`** — Exit the entire parent function via its secondary path (only valid if the parent function declares a compatible abort type).

```cpp
// With a named binder (AbortType is String)
Int x = parse("abc") ? err {
    log("Failed: " + err)    // err is of type String
    resolve 0                 // substitute 0 as the result
}

// With no binder (AbortType is Void)
Int x = tryRead("file.txt") ? {
    resolve 0                 // failure carries no info
}

// Propagating the abort upward (parent must declare ? String)
Int ? String process(String input) pure {
    Int x = parse(input) ? err { abort err }
    return x * 2
}

// Mixed: recover some errors, propagate others
Int ? Codes load(String fileName) {
    String content = read(fileName) ? err {
        if (err == Codes::FileNotFound) { resolve "default" }
        abort err
    }
    return content.toInt()
}
```

### 5.2 Exhaustiveness Rule
The compiler performs **exhaustiveness checking** on every `?` handler block. If any code path through the block fails to `resolve`, `return`, or `abort`, it is a **compile-time error**.

```cpp
// COMPILER ERROR: not all paths resolve
Int x = parse("abc") ? err {
    log(err)
    // Missing resolve/return/abort!
}
```

### 5.3 The `??` Shorthand (Value Coalescing)
For the common case of "resolve with a default value if aborted", the `??` operator provides a concise inline shorthand. It is syntactic sugar for a `?` block containing only `resolve`:

```cpp
// Sugar
Int x = parse("abc") ?? 0

// Equivalent to
Int x = parse("abc") ? _ { resolve 0 }
```

`??` is valid regardless of the abort type, including `Void`.

### 5.4 Omitting the Result (`Void` Primary Return)
Just as a call to a `Void`-returning function must not be assigned to a variable, a call whose abort path is handled must not assign the abort binder when the abort type is `Void`:

```cpp
// Primary Void: result is not assigned (mirrors Void return)
print("hello")              // correct
String s = print("hello")  // COMPILER ERROR

// Secondary Void: binder is omitted in the handler
Int x = tryRead("file.txt") ? {
    resolve 0
}
```

---

## 6. The `resolve` Keyword

`resolve` is a **block-level return** that substitutes a value as the result of the aborted call expression. It does **not** exit the parent function. It is only valid inside a `?` handler block.

| Keyword   | Exits           | Valid In               | Purpose                                       |
|-----------|-----------------|------------------------|-----------------------------------------------|
| `resolve` | Handler block   | `?` handler only       | Substitute a success value                    |
| `return`  | Parent function | Anywhere               | Exit parent via primary path                  |
| `abort`   | Parent function | Anywhere (if declared) | Exit parent via secondary path                |
| `abort`   | Parent function | Anywhere (if declared) | Exit parent via secondary path (Void payload) |

```cpp
String ? Codes process(String fileName) {
    // resolve: only exits the handler, process() continues
    String content = read(fileName) ? err {
        resolve "default"
    }

    // return: exits process() entirely via primary path
    String backup = read("backup.txt") ? err {
        return "hardcoded fallback"
    }

    // abort: exits process() entirely via secondary path
    String final = read("final.txt") ? err {
        abort Codes::Unrecoverable
    }

    return content + backup + final
}
```

---

## 7. Compiler Guarantees

| Guarantee | Description |
|---|---|
| **No Unhandled Aborts** | Every call to an abortable function must have a `?` handler or `??`. Unhandled aborts are compile-time errors. |
| **Exhaustiveness** | Every path through a `?` handler block must `resolve`, `return`, or `abort`. |
| **Type Safety** | The `resolve` expression must match the `ReturnType` of the aborted call. The `abort` expression must match the `AbortType` of the parent function. |
| **Zero Runtime Overhead** | When handled immediately at the call site, aborts are compiled to a conditional jump. No heap allocation, no stack unwinding, no union storage. |
| **No Implicit Abort Type Dropping** | A function's abort type is part of its structural type. It cannot be silently dropped when assigning to a function variable. |
| **Abort-Free Guarantee** | A function with no `?` in its signature is **statically guaranteed** to never abort. The compiler verifies this transitively. |

---

## 8. Summary of Syntax

```
// Function declaration
ReturnType ? AbortType Name(Params) [pure] { ... }
ReturnType ? AbortType Name(Params) [pure] => expr

// Function type
(ParamTypes) [pure] -> ReturnType ? AbortType

// Handler block (AbortType is T)
expr ? binder { ... resolve Value ... }

// Handler block (AbortType is Void)
expr ? { ... resolve Value ... }

// Coalescing shorthand
expr ?? DefaultValue

// Inside a handler block
resolve Value     // Substitute value; continue parent function
return Value      // Exit parent function via primary path
abort Value       // Exit parent function via secondary path
abort             // Exit parent function via secondary path (Void abort type)
```

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| `?` separates return and abort types | Creates visual and syntactic symmetry between declaration and call site. |
| Abort type is structural, not behavioral | Changing the abort type changes the call contract. It cannot be dropped implicitly. |
| `pure` is behavioral, not structural | A pure function is a strict subset of a non-pure function. Upcasting is always safe. |
| `resolve` is a distinct keyword | Prevents ambiguity between "exit this block" and "exit this function." `return` and `abort` always refer to the parent function. |
| `Void` abort type instead of empty `?` | Acknowledges that failure is a real, explicit code path. Mirrors the meaning of `Void` as a primary return type. |
| No stored `T?E` values | Abortability is a control flow construct, not a data construct. Developers who need to store success/failure use `Union<T,E>` explicitly. |
| No implicit default initialization | Variables must be consciously set. The compiler uses Control Flow Graph analysis to guarantee all paths initialize a variable before use, with zero runtime overhead. |

---

## 10. Comparison With Other Languages

This section compares Zane's error handling model against established languages. The goal is to highlight where Zane improves upon existing designs and where it makes deliberate, principled trade-offs.

---

### 10.1 Comparison Table

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
| Purity annotations | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

---

### 10.2 Zane vs. C (Return Codes)

C is the most primitive error handling model in widespread use. Functions signal failure by returning a sentinel value (usually `-1` or `NULL`), and set a global `errno` variable to specify the reason.

**C:**
```c
FILE *f = fopen("file.txt", "r");
if (f == NULL) {
    fprintf(stderr, "Error: %d\n", errno);
    return -1;
}
```

**Zane:**
```cpp
File ? Codes f = openFile("file.txt") ? err {
    log("Error: " + err)
    abort err
}
```

| Problem in C | How Zane Solves It |
|---|---|
| The compiler does not enforce that you check the return value. You can silently ignore `NULL`. | Every abortable call **must** have a `?` handler. Unhandled aborts are compile-time errors. |
| The error type (`errno`) is a global variable, not part of the function signature. You must read documentation to know what errors a function can produce. | The abort type is part of the function signature (`Int ? Codes`). The compiler and the type system document it for you. |
| Error codes and success values share the same return channel, requiring magic sentinel values (`-1`, `NULL`). | The primary and secondary paths are completely separate. There are no sentinel values. |
| Functions cannot be composed without manual intermediate checks. | `??` and inline `?` blocks allow safe composition in a single expression. |

---

### 10.3 Zane vs. Go (Multiple Return Values)

Go formalizes return codes by allowing multiple return values. The idiomatic pattern is to return `(Value, error)`. While more structured than C, it is still fundamentally a manual, convention-based system.

**Go:**
```go
content, err := os.ReadFile("file.txt")
if err != nil {
    return "", err
}
```

**Zane:**
```cpp
String content = readFile("file.txt") ? err { abort err }
```

| Problem in Go | How Zane Solves It |
|---|---|
| The compiler does not enforce that you check the `error` return. Assigning to `_` silently discards it. | Every abortable call **must** have a `?` handler. There is no equivalent of `_` for discarding an abort. |
| The `if err != nil` pattern is repeated thousands of times in a typical Go codebase, creating significant boilerplate. | The `?` handler and `??` shorthand replace `if err != nil` with a concise, expressive inline construct. |
| Error propagation (`return "", err`) requires manually constructing zero values for all other return values. | `abort err` propagates the error upward cleanly, with no need to construct dummy success values. |
| The `error` type is a generic interface. The type system does not distinguish between functions that can fail with an `IOError` vs. a `ParseError`. | The abort type is fully typed (`? IOError`, `? ParseError`). The compiler enforces compatibility. |
| Functions returning `(T, error)` cannot be passed as a callback to a function expecting `T`. | Zane's abort type is part of the function type signature, making higher-order functions fully type-safe. |

---

### 10.4 Zane vs. Java (Checked Exceptions)

Java is the most well-known language to attempt to enforce error handling via **Checked Exceptions**. If a method can throw a checked exception, it must declare it with `throws`, and callers must either catch it or propagate it.

**Java:**
```java
public String readFile(String path) throws IOException {
    if (!Files.exists(Path.of(path))) throw new IOException("Not found");
    return Files.readString(Path.of(path));
}

// Call site
try {
    String content = readFile("file.txt");
} catch (IOException e) {
    System.out.println("Failed: " + e.getMessage());
    content = "default";
}
```

**Zane:**
```cpp
String ? IOError readFile(String path) {
    if (!fs::exists(path)) abort IOError::NotFound
    return fs::read(path)
}

String content = readFile("file.txt") ? err {
    log("Failed: " + err)
    resolve "default"
}
```

| Problem in Java | How Zane Solves It |
|---|---|
| `try/catch` blocks are physically separated from the call site, forcing the programmer to jump between distant code locations to understand control flow. | The `?` handler is always **adjacent to the call**. Error handling and the call site are one unified expression. |
| Exceptions use **stack unwinding**, which is expensive. The JVM must walk the entire call stack to find a matching `catch` block at runtime. | Zane's abort paths are compiled to **conditional jumps**. There is no stack unwinding and no runtime overhead. |
| Unchecked exceptions (`RuntimeException`) are not declared and can propagate silently, escaping the type system entirely. | There are no unchecked aborts in Zane. All abort paths are declared and enforced by the compiler. |
| Inline recovery is not possible. You cannot write `String s = readFile() catch { "default" }` in Java. | Zane's `??` and `?` handler enable clean, inline, expression-level recovery. |
| The `catch` block introduces a new scope, making it awkward to use the result variable after recovery. | Because `resolve` substitutes the value directly into the expression, the variable is available immediately in the same scope. |

---

### 10.5 Zane vs. Python (Unchecked Exceptions)

Python uses unchecked exceptions. Any function can `raise` any exception at any time, and the type system provides no information about what a function might raise.

**Python:**
```python
try:
    content = read_file("file.txt")
except FileNotFoundError as e:
    print(f"Failed: {e}")
    content = "default"
```

**Zane:**
```cpp
String content = readFile("file.txt") ? err {
    log("Failed: " + err)
    resolve "default"
}
```

| Problem in Python | How Zane Solves It |
|---|---|
| The type system has no knowledge of what exceptions a function raises. You must read documentation or source code. | The abort type (`? IOError`) is part of the function's type signature. It is self-documenting and compiler-verified. |
| Any function can raise any exception. There is no way to statically guarantee a function cannot fail. | Functions without `?` in their signature are **statically guaranteed** by the compiler to never abort. |
| Exceptions unwind the stack, which is expensive in performance-critical code. | Zane uses zero-cost conditional jumps. |
| `try/catch` blocks are disconnected from the call expression, making functional composition difficult. | Zane's `?` handler and `??` are **part of the expression**, enabling clean function composition. |

---

### 10.6 Zane vs. Rust (`Result<T, E>`)

Rust is the closest language in philosophy to Zane. Its `Result<T, E>` type enforces error handling at the type level. The `?` operator propagates errors upward. It is the most rigorous error handling system in any mainstream language.

**Rust:**
```rust
fn read_file(path: &str) -> Result<String, IOError> {
    if !Path::new(path).exists() {
        return Err(IOError::NotFound);
    }
    Ok(fs::read_to_string(path)?)
}

let content = read_file("file.txt").unwrap_or_else(|e| {
    eprintln!("Failed: {}", e);
    "default".to_string()
});
```

**Zane:**
```cpp
String ? IOError readFile(String path) {
    if (!fs::exists(path)) abort IOError::NotFound
    return fs::read(path)
}

String content = readFile("file.txt") ? err {
    log("Failed: " + err)
    resolve "default"
}
```

| Difference | Rust | Zane |
|---|---|---|
| **Error representation** | `Result<T, E>` is a real enum value stored in a register or on the stack at all times. | The abort path is a **control flow construct**. No `Result` value is created at the call site unless stored explicitly in a `Union<T,E>`. |
| **Inline recovery** | `.unwrap_or()`, `.unwrap_or_else()`, `.map_err()` — functional but verbose chained methods. | `?` handler block and `??` — block-scoped, readable, and supports multi-line logic naturally. |
| **Propagation** | The `?` operator propagates automatically. | `? err { abort err }` is explicit. A shorthand propagation operator may be considered in a future version. |
| **Storing errors** | `Result<T, E>` can be stored, passed, and returned as a value naturally. | Storing requires an explicit `Union<T, E>`. Abortability is not a storable type constructor. |
| **Closure conflicts** | `.unwrap_or_else()` uses closures, which can conflict with the borrow checker in complex scenarios. | `?` handler blocks are part of the parent scope. There are no closure captures and no borrow checker conflicts. |
| **Purity** | No built-in purity annotations. | `pure` is a first-class modifier, statically verified by the compiler. |

Zane and Rust share the same core philosophy: **errors are explicit, typed, and compiler-enforced.** Zane's primary contribution over Rust is making the call-site handling **expression-oriented and block-scoped** rather than value-oriented and method-chained, resulting in cleaner recovery logic for complex multi-step error handling.

---

### 10.7 Zane vs. Swift (`throws`)

Swift uses a `throws`/`try`/`catch` model that is superficially similar to Zane's but differs in important ways.

**Swift:**
```swift
func readFile(_ path: String) throws -> String {
    guard FileManager.default.fileExists(atPath: path) else {
        throw FileError.notFound
    }
    return try String(contentsOfFile: path)
}

let content: String
do {
    content = try readFile("file.txt")
} catch {
    print("Failed: \(error)")
    content = "default"
}
```

**Zane:**
```cpp
String ? FileError readFile(String path) {
    if (!fs::exists(path)) abort FileError::NotFound
    return fs::read(path)
}

String content = readFile("file.txt") ? err {
    log("Failed: " + err)
    resolve "default"
}
```

| Difference | Swift | Zane |
|---|---|---|
| **Error type in signature** | `throws` declares that a function can fail, but **does not declare the error type**. The thrown type is always `any Error`. | The abort type is **fully typed** (`? FileError`). The compiler enforces type compatibility between the `abort` expression and the declared abort type. |
| **Call site structure** | `do { try ... } catch { ... }` — the success and failure paths are in separate blocks, requiring a pre-declared variable for the result. | `?` handler is an inline expression. No pre-declaration needed. The result is available immediately in the same scope. |
| **Stack unwinding** | Swift exceptions use stack unwinding with a runtime cost. | Zane uses zero-cost conditional jumps. |
| **`??` coalescing** | `try? readFile("file.txt") ?? "default"` — works, but `try?` silently discards the error type entirely. | `readFile("file.txt") ?? "default"` — the `??` coalesces cleanly. The abort type is still known to the compiler even when using `??`. |

---

### 10.8 Zane vs. Zig (Error Unions)

Zig is the language most architecturally similar to Zane. Zig uses **Error Union types** (`ErrorType!ReturnType`) and a `catch` construct for inline recovery.

**Zig:**
```zig
fn readFile(path: []const u8) ![]const u8 {
    if (!fileExists(path)) return error.NotFound;
    return fs.readFile(path);
}

const content = readFile("file.txt") catch |err| blk: {
    std.debug.print("Failed: {}\n", .{err});
    break :blk "default";
};
```

**Zane:**
```cpp
String ? FileError readFile(String path) {
    if (!fs::exists(path)) abort FileError::NotFound
    return fs::read(path)
}

String content = readFile("file.txt") ? err {
    log("Failed: " + err)
    resolve "default"
}
```

| Difference | Zig | Zane |
|---|---|---|
| **Type order** | `ErrorType!ReturnType` — error type on the left, return type on the right. | `ReturnType ? AbortType` — return type on the left, abort type on the right. This mirrors C-family conventions where the primary return type comes first. |
| **Block labels** | `catch \|err\| blk: { break :blk value }` — requires a labeled block and `break` to produce a value. | `? err { resolve value }` — the dedicated `resolve` keyword eliminates the need for block labels, improving readability. |
| **Void abort type** | Zig infers the error set automatically in many cases. | Zane requires explicit `Void` to acknowledge that failure is a real code path, even if it carries no payload. |
| **Purity** | No purity annotations. | `pure` is a first-class, statically verified modifier. |
| **Syntax family** | Zig has its own syntax distinct from C. | Zane uses C++-style syntax, lowering the learning curve for C/C++/Java developers. |

Zane's primary improvement over Zig at the call site is the `resolve` keyword, which cleanly replaces Zig's labeled block/break pattern and eliminates the ambiguity between "exit this block" and "exit this function."

---

### 10.9 Summary: Zane's Position in the Landscape

Zane sits at a unique intersection in the error handling design space. It combines:

- **The static enforcement of Rust and Zig** — unhandled aborts are compile-time errors.
- **The call-site ergonomics of Swift** — inline, expression-oriented recovery without pre-declaring variables.
- **The zero-cost model of C and Zig** — conditional jumps, no stack unwinding, no forced union storage.
- **The typed error contracts of Rust and Zig** — the abort type is part of the function's structural type.
- **A unique innovation** — the `resolve` keyword, which cleanly separates "substitute a value here" from "exit this function," an ambiguity that every other language leaves to convention or labeled blocks.

No existing mainstream language achieves all of these simultaneously. Zane's design is the result of treating error handling as a **first-class control flow primitive** rather than an afterthought bolted onto an existing type system.
