# Zane Language Comparisons

This document compares Zane's design choices to those of other widely-used languages. It is a companion to the individual spec documents, which describe what the language does, and to [`rationale.md`](rationale.md), which explains why.

---

## 1. Memory Model

### 1.1 Ownership and References

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner | ✅ | ✅ | ❌ | ✅ |
| Refs nulled on destroy | ✅ | ❌ | via `weak_ptr` | ❌ |
| Ownership by default | ✅ | ❌ | ❌ | ✅ |
| Non-ownership opt-in (`ref`) | ✅ | ❌ | ❌ | ✅ (`&`) |
| Inline element storage | Default | ❌ | ❌ | manual |
| Ownership cycles possible | ❌ | ❌ | ✅ | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Ref counting | ❌ | ❌ | ✅ | via `Rc`/`Arc` |
| Garbage collector | ❌ | ❌ | ❌ | ❌ |
| Move safety (anchor) | ✅ | ❌ | ❌ | compile-time |
| Refs as class fields | ✅ | ✅ | ✅ | ✅ |

### 1.2 Memory Layout

| Property | Zane | GC language (JVM, Go) | Rust | C/C++ |
|---|---|---|---|---|
| Fragmentation | None | Managed by GC | Allocator-dependent | Allocator-dependent |
| GC pauses | None | Yes | None | None |
| Allocation cost | O(1) always | Fast (bump alloc + GC) | Allocator-dependent | Allocator overhead |
| Deallocation cost | O(1) always | Deferred to GC | Allocator-dependent | Coalescing overhead |
| Random free order cost | Same as sequential | Deferred | Same as sequential | Higher (coalescing) |
| Destruction timing | Deterministic | Non-deterministic | Deterministic | Manual / RAII |
| Dangling pointer risk | None (refs via anchors) | None | None (compile-time) | Yes |
| Move safety | O(1) anchor update | GC-managed | compile-time (Pin) | Manual |
| Ref nulling on destroy | O(1) via anchor stack | N/A | N/A | N/A |
| Lifetime annotations | None | None | Required | None |
| Struct/class layout | Declaration order, auto bool-pack | JVM-managed | Repr-controlled | Manual / compiler |
| Bool packing | Automatic | No | Manual (`bitflags`) | Manual (bitfields) |
| Inline list storage | Default | No | Manual (`Vec<T>` inline) | Manual |
| Per-type memory isolation | No (shared free stacks) | No | No | No |

---

## 2. Error Handling

### 2.1 Feature Matrix

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

### 2.2 Zane vs. C (Return Codes)

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

### 2.3 Zane vs. Go (Multiple Return Values)

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

### 2.4 Zane vs. Java (Checked Exceptions)

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

### 2.5 Zane vs. Python (Unchecked Exceptions)

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

### 2.6 Zane vs. Rust (`Result<T, E>`)

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

### 2.7 Zane vs. Swift (`throws`)

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

### 2.8 Zane vs. Zig (Error Unions)

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

### 2.9 Summary

Zane sits at a unique intersection in the error handling design space. It combines:

- **The static enforcement of Rust and Zig** — unhandled aborts are compile-time errors.
- **The call-site ergonomics of Swift** — inline, expression-oriented recovery without pre-declaring variables.
- **The zero-cost model of C and Zig** — conditional jumps, no stack unwinding, no forced union storage.
- **The typed error contracts of Rust and Zig** — the abort type is part of the function's structural type.
- **A single effect marker** — `mut` on methods is the only user-facing annotation; no `pure`, `readonly`, or effect lists.
- **A unique innovation** — the `resolve` keyword, which cleanly separates "substitute a value here" from "exit this function," an ambiguity that every other language leaves to convention or labeled blocks.
