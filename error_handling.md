# Zane Error Handling

This document specifies Zane's error handling model: bifurcated return paths, `?` handler blocks, `??` shorthand, and compiler guarantees around abortability.

> **See also:** [`purity.md`](purity.md) §7 for the relationship between effects and abortability. [`syntax.md`](syntax.md) §5 for the canonical surface syntax.

---

## 1. Overview

Zane uses a **Bifurcated Return Path** model in which success and failure are both explicit and statically typed.

- **`Primary path`.** The normal return path uses the return type on the left of `?`.
- **`Abort path`.** The failure path uses the abort type on the right of `?`.
- **`Mandatory handling`.** Every abortable call must be handled or explicitly propagated.

---

## 2. Core Concepts

### 2.1 Abort types are part of the signature
An abortable function declares:

```
ReturnType ? AbortType
```

A declaration with no `? AbortType` cannot abort.

### 2.2 `Void` abort type
If failure carries no payload, the abort type is `Void`. In that case `abort` is written without an argument.

### 2.3 Abortability is orthogonal to `mut`
Abortability and mutation are independent. A method may be:

- non-`mut`, non-aborting
- non-`mut`, aborting
- `mut`, non-aborting
- `mut`, aborting

### 2.4 Abort type is structural, not behavioral
Changing a function's abort type changes its function type. Abort types cannot be silently discarded when functions are passed around as values.

---

## 3. Call-Site Handling

### 3.1 `?` handler blocks
An abortable call must attach a handler block:

```zane
value Int
value = parse("42") ? err {
    resolve Int(0)
}
```

When the abort type is `Void`, the binder is omitted:

```zane
done Bool
done = tryFinish() ? {
    resolve false
}
```

### 3.2 Handler outcomes are exhaustive
Every path through a handler block must end in one of:

- `resolve ...`
- `return ...`
- `abort ...`

Falling through a handler block is a compile-time error.

### 3.3 `??` is resolve-only shorthand
`expr ?? fallback` desugars to a `?` block that only resolves a default value.

```zane
count Int
count = parse("abc") ?? Int(0)
```

### 3.4 `Void` primary returns are not assignable
Calls whose primary return type is `Void` may not be assigned to variables. When such calls are abortable, the handler still attaches to the call expression itself.

---

## 4. The `resolve` Keyword

`resolve` substitutes a value for the aborted call expression. It exits only the handler block.

| Keyword | Exits | Purpose |
|---|---|---|
| `resolve` | handler block | provide a value for the failed call expression |
| `return` | parent function | leave via the primary return path |
| `abort` | parent function | leave via the abort path |

When the primary return type is `Void`, `resolve` takes no value.

---

## 5. Connection to the Effect Model

Abortability is not itself a side effect. A function can be pure and abortable, impure and non-aborting, or both. The effect system and the abort system are analyzed independently and combined by the compiler at the call site.

---

## 6. Compiler Guarantees

| Guarantee | Meaning |
|---|---|
| No unhandled aborts | Every abortable call must have a `?` or `??` handler, or be explicitly propagated in a parent that declares a compatible abort type. |
| Exhaustive handlers | Every path through a handler block must terminate with `resolve`, `return`, or `abort`. |
| Return-path type safety | `resolve` values must match the primary return type; `abort` values must match the parent function's abort type. |
| Abort-free functions stay abort-free | A function with no abort type is verified never to abort transitively. |
| No implicit abort-type dropping | Function-value assignments must preserve abort types exactly. |

---

## 7. Language Comparisons

### 7.1 Feature matrix

| Feature | Zane | C | Go | Java | Python | Rust | Swift | Zig |
|---|---|---|---|---|---|---|---|---|
| Unhandled failures are compile-time errors | ✅ | ❌ | ❌ | ⚠️ checked only | ❌ | ✅ | ✅ | ✅ |
| Failure type is part of the function type | ✅ | ❌ | ❌ | ⚠️ exception list only | ❌ | ✅ | ❌ | ✅ |
| Stack unwinding required | ❌ | ❌ | ❌ | ✅ | ✅ | ❌ | ✅ | ❌ |
| Inline recovery at call site | ✅ | ❌ | ❌ | ⚠️ verbose | ⚠️ verbose | ⚠️ verbose | ✅ | ✅ |
| Abort-free functions are statically guaranteed | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ✅ |

### 7.2 Zane vs. C

C typically uses sentinel return values and out-of-band global error state.

**C:**
```c
FILE *f = fopen("file.txt", "r");
if (f == NULL) {
    return -1;
}
```

**Zane:**
```zane
file File
file = fs:open("file.txt") ? err {
    abort err
}
```

| Problem in C | How Zane solves it |
|---|---|
| Sentinel values can be ignored | Abortable calls must be handled. |
| Error state is often global | Failure type is carried in the signature. |
| Success and failure share one channel | Primary and abort paths are distinct. |

### 7.3 Zane vs. Go

Go exposes errors as a second return value that callers may ignore.

**Go:**
```go
content, err := os.ReadFile("file.txt")
if err != nil {
    return "", err
}
```

**Zane:**
```zane
content String
content = fs:readFile("file.txt") ? err { abort err }
```

| Problem in Go | How Zane solves it |
|---|---|
| Errors are optional to check | Handlers are mandatory. |
| Boilerplate `if err != nil` blocks are repetitive | `?` and `??` keep recovery adjacent to the call. |
| Error type is conventionally broad | Abort types are precise and structural. |

### 7.4 Zane vs. Java/Python exceptions

Java and Python use exceptions and stack unwinding.

**Java:**
```java
try {
    String content = readFile("file.txt");
} catch (IOException e) {
    content = "default";
}
```

**Zane:**
```zane
content String
content = fs:readFile("file.txt") ? err {
    resolve "default"
}
```

| Problem in exception models | How Zane solves it |
|---|---|
| Recovery is separated from the call site | Handler syntax is adjacent to the call. |
| Unwinding is runtime control flow | Zane uses explicit typed branches instead. |
| Unchecked exceptions escape type systems | Abort paths are always declared. |

---

## 8. Design Rationale

| Decision | Rationale |
|---|---|
| `?` splits primary and abort paths | Makes success and failure equally visible in signatures. |
| Handler blocks are mandatory | Prevents silent propagation or ignored failure. |
| `resolve` is distinct from `return` | Separates local recovery from exiting the parent function. |
| `Void` is an explicit abort type | Keeps payload-free failure visible rather than implicit. |
| `??` is only shorthand, not a new mechanism | Preserves one mental model while reducing boilerplate. |
| Abortability is structural | Function values must preserve exact call contracts. |
| No required user-facing `Result` wrapper at the call site | Treats abortability primarily as typed control flow at the language surface; implementations may lower it however they choose. |

---

## 9. Summary

| Concept | Rule |
|---|---|
| Abortable function | Declares `ReturnType ? AbortType` |
| `?` handler | Mandatory for every abortable call |
| Handler paths | Must end with `resolve`, `return`, or `abort` |
| `??` | Shorthand for resolve-with-default |
| `resolve` | Exits only the handler block |
| Abort-free function | Statically guaranteed not to abort |
