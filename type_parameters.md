# Zane Type Parameters

This document specifies Zane's type-parameter system, including const-parameterized types and the `Array[size]<T>` primitive.

> **See also:** [`syntax.md`](syntax.md) §2 for parameter syntax. [`memory_model.md`](memory_model.md) §6 for memory layout context.

---

## 1. Overview

Zane supports both type parameters and const-int parameters, with distinct syntax and roles.

- **`Two parameter kinds`.** Type parameters use `<T>`; const parameters use `[name]`.
- **`Const params are literal`.** Use-site const parameters are always integer literals baked into identifiers.
- **`Array is primitive`.** `Array[size]<T>` is the single compiler-defined fixed-size container.

---

## 2. Parameter Kinds

### 2.1 Type parameters
Type parameters are declared with angle brackets and range over types:

```zane
struct Box<T> { value T }
```

### 2.2 Const parameters
Const parameters are declared with square brackets and range over compile-time integers:

```zane
struct Matrix[rows]X[cols]<T> { ... }
```

### 2.3 Kinds are distinct
Type parameters and const parameters live in different kinds. A const parameter **MUST NOT** be supplied where a type parameter is expected and vice versa.

---

## 3. Definition-Site Binders

Const parameters are bound in the type name itself:

```zane
struct Matrix[rows]X[cols]<T> {
    data Array[rows]<Array[cols]<T>>
}
```

Within the type body, the binder name refers to the compile-time integer.

---

## 4. Method Signatures and `this` Binders

When a method binds const parameters in its `this` type, the method is implicitly generic over those parameters:

```zane
Array[cols]<T> rowAt(this Matrix[rows]X[cols]<T>, i Int) { ... }
```

`rows` and `cols` in the method body refer to the binders introduced by `this`.

---

## 5. Use-Site Forms and Identifier Rules

### 5.1 Const arguments are baked into identifiers
Use sites supply const arguments as integer literals embedded in the type name:

```zane
Matrix10X20<Float>
```

### 5.2 Numbers in identifiers are otherwise illegal
Any digit in an identifier is interpreted as a const-parameter argument. Identifiers **MUST NOT** contain digits unless they are part of a const-parameterized type name.

```zane
player2      // ILLEGAL
Matrix10X20  // ok
```

---

## 6. Const-Parameter Restrictions

### 6.1 Literal-only at use site
Const arguments at use site **MUST** be integer literals. Runtime values are illegal.

```zane
Matrix10X20<Float>   // ok
Matrix[n]X[m]<Float> // ILLEGAL
```

### 6.2 No arithmetic in type arguments
Arithmetic on const parameters in type arguments is **not specified**. Expressions such as `Array[rows*cols]<T>` are currently illegal.

---

## 7. The `Array[size]<T>` Primitive

### 7.1 Compiler-defined layout
`Array[size]<T>` is a compiler primitive representing `size` contiguous elements of `T`. Its size is `size * sizeof(T)` bytes with no header.

### 7.2 Array is the base case
Other fixed-size container types (e.g., vectors, matrices) are defined in terms of `Array` and do not require compiler support.

---

## 8. Deferred Features

The following are intentionally not specified in this version of the spec:

- arithmetic on const parameters in type positions
- dynamic container types such as `List<T>` and `Map<K,V>`
- bounds-checking rules for element access APIs
- named lane access (`.x`, `.y`, `.z`, `.w`)

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| Separate `<T>` and `[n]` | Keeps kinds explicit and avoids dependent-type complexity. |
| Literal const arguments | Ensures all concrete types are known at compile time. |
| Digits restricted to const params | Makes identifiers context-free and unambiguous. |
| `Array` as the only primitive | Minimizes compiler surface area; other types are definable in Zane. |
