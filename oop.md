# Zane Object-Oriented Model

This document specifies Zane's object model: classes, structs, constructors, methods, packages, and method-call resolution.

> **See also:** [`memory_model.md`](memory_model.md) §2 for ownership rules. [`purity.md`](purity.md) §2 for `mut`. [`syntax.md`](syntax.md) §1 for declarations.

---

## 1. Overview

Zane's object model is built from a small set of orthogonal constructs.

- **`Classes and structs`.** Classes are heap-allocated and owned; structs are value types stored inline.
- **`Free-function methods`.** Methods are package-scope functions whose first parameter is `this`.
- **`Explicit mutation`.** Mutating methods are marked `mut` and called with `!`.
- **`Package namespacing`.** Members are namespaced as `Package$member`, with controlled shorthand for methods.

---

## 2. Types and Packages

### 2.1 Packages are explicit namespaces
Every declaration belongs to a package. Package members are referenced as `Package$member` unless a shorter, unqualified method form is allowed (see §5).

### 2.2 Classes declare fields only
Class bodies contain only field declarations. They do not contain methods or constructors.

```zane
package Graph

class Node {
    _id Int
    label String
}
```

### 2.3 Structs are value types
Structs are stored inline and may not contain class fields or `ref` fields.

```zane
package Math

struct Vec2 {
    x Float
    y Float
}
```

### 2.4 Field visibility by naming convention
Fields beginning with `_` are private to the defining package’s methods. All other fields are public.

---

## 3. Constructors and Initialization

### 3.1 Constructors are package-scope declarations
A constructor is a package-scope declaration named after the type. It has no `this` parameter.

### 3.2 `init{ }` injects fields directly
Constructors return `init{ }` to build an instance. Every field must be provided.

```zane
package Graph

Node(id Int, label String) {
    return init{ _id: id, label: label }
}
```

### 3.3 Typed initialization from expressions
A declaration may initialize from any expression value without using constructor syntax:

```zane
x Int = random()
```

---

## 4. Methods and Calls

### 4.1 Methods are free functions with `this`
A method is a package-scope function whose first parameter is named `this`. `this` **MUST** be the first parameter.

```zane
Void inspect(this Node) { ... }
Void increment(this Node) mut { ... }
```

### 4.2 Call markers `:` and `!`
Non-mutating methods are called with `:`. Mutating methods are called with `!` and must be declared `mut`.

```zane
node:inspect()
node!increment()
```

Calling a `mut` method with `:` is illegal.

### 4.3 Desugaring

```
r:m(a, b)     → ResolvedPkg$m(r, a, b)
r:Pkg$m(a)    → Pkg$m(r, a)
```

The explicit form `Pkg$m(r, ...)` is always legal and unambiguous.

---

## 5. Method Name Resolution

### 5.1 Unqualified lookup
For `receiver:methodName(...)`, the compiler resolves in this order:

1. The receiver type’s **home package**
2. The **current package**

If no candidate matches, it is a compile error. If multiple candidates remain after overload resolution, it is a compile error and the call must be qualified.

### 5.2 Qualified calls for extensions
Extension methods from other packages must be called with explicit qualification:

```zane
vec:Physics$kineticEnergy()
```

---

## 6. Pipe Operator

`|` pipes the expression on the right into the **last argument** of the call on the left.

```zane
startGame(state)|{
    ...
}
```

`spawn` may apply to a piped call because the result is still a call expression:

```zane
spawn startGame(state)|{ ... }
```

---

## 7. Design Rationale

| Decision | Rationale |
|---|---|
| Classes contain fields only | Keeps data layout explicit and avoids mixing behavior with storage. |
| Methods are free functions | Preserves explicit namespacing while still enabling `:` call syntax. |
| `:` and `!` call markers | Makes mutation explicit at the call site without ref qualifiers. |
| Home+current package lookup | Keeps method calls locally reasonable and independent of imports. |
| Pipe to last argument | Supports block/lambda chaining without new syntax forms. |
