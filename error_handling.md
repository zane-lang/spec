# Zane Error Handling

This document specifies Zane's error handling model: bifurcated return paths, `?` handlers, and abort/resolve semantics.

> **See also:** [`syntax.md`](syntax.md) §3 for function declarations and §6 for error handling syntax.

---

## 1. Overview

Zane uses a **Bifurcated Return Path** model: every function has a primary return path and an optional abort path.

- **`Primary path`.** The normal return type on the left of `?`.
- **`Abort path`.** The failure return type on the right of `?`.
- **`Mandatory handling`.** Every abortable call must be handled at the call site.

---

## 2. Core Concepts

### 2.1 Abort types
A function that can fail declares an abort type:

```
ReturnType ? AbortType
```

A function with no `?` in its signature **MUST NOT** abort.

### 2.2 `Void` abort type
If a function can fail without a payload, it declares `? Void`. The `abort` keyword is used with no argument.

### 2.3 Abortability is orthogonal to `mut`
`mut` and abortability are independent. A method may be mutating or non-mutating and may abort or not abort in any combination.

---

## 3. Call-Site Handling

### 3.1 The `?` handler block
Abortable calls **MUST** attach a `?` handler block. The block receives the abort value as a binder unless the abort type is `Void`.

```zane
value Int
value = parse("42") ? err {
    resolve Int(0)
}
```

### 3.2 Exhaustiveness rule
Every path through a handler block must `resolve`, `return`, or `abort`. Missing a terminal is a compile-time error.

### 3.3 The `??` shorthand
`??` is shorthand for a handler that only resolves a default value:

```zane
x Int
x = parse("abc") ?? Int(0)
```

### 3.4 `Void` primary returns
Calls that return `Void` must not be assigned. If the abort type is `Void`, the handler omits the binder:

```zane
log:write("hello")
log:write("hello") ? { resolve } // ok, abort type is Void
```

---

## 4. The `resolve` Keyword

`resolve` substitutes a value for the aborted call expression. It exits only the handler block, not the parent function. When the primary return type is `Void`, `resolve` takes no value.

| Keyword | Exits | Purpose |
|---|---|---|
| `resolve` | handler block | substitute a success value |
| `return` | parent function | return via primary path |
| `abort` | parent function | return via abort path |

---

## 5. Design Rationale

| Decision | Rationale |
|---|---|
| Bifurcated return paths | Makes failure explicit and statically enforced. |
| Mandatory handlers | Eliminates unchecked error flows. |
| `??` shorthand | Keeps common defaulting code concise without hiding control flow. |
| `resolve` vs `return` | Distinguishes local recovery from function exit. |
