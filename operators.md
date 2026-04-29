# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, derived operators, precedence, and boolean keywords.

> **See also:** [`syntax.md`](syntax.md) §5 for operator tokens and expression forms. [`purity.md`](purity.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Fixed operator set`.** Operators are not user-defined tokens; only the built-in set exists.
- **`Derived operators`.** Some operators are defined strictly in terms of others and cannot be reimplemented.
- **`Short-circuit logic`.** Boolean `and`/`or` are keywords, not operators, to preserve control flow semantics.

---

## 2. Operator Set

### 2.1 Primitive operators
Primitive operators are implementable and define the operator surface area:

| Operator | Arity | Signature |
|---|---|---|
| `~` | unary prefix | `T ~(value T)` |
| `*` | binary | `T *(left T, right T)` |
| `/` | binary | `T /(left T, right T)` |
| `+` | binary | `T +(left T, right T)` |
| `==` | binary | `Bool ==(left T, right T)` |

### 2.2 Derived operators
Derived operators are fixed desugarings and are **not** independently implementable:

| Operator | Desugars to |
|---|---|
| `a - b` | `a + ~b` |
| `a ~= b` | `~(a == b)` |

### 2.3 Boolean keywords
`and` and `or` are **keywords**, not operators. They are short-circuiting and therefore not implementable as regular functions.

```zane
if ready and check() { ... }
if ok or fallback() { ... }
```

`or` is defined as a law: `a or b = ~(~a and ~b)`.

---

## 3. Precedence and Associativity

| Level (high → low) | Operators | Associativity |
|---|---|---|
| 1 | `~` | — |
| 2 | `*` `/` | left |
| 3 | `+` `-` | left |
| 4 | `==` `~=` | non-associative |

`==` and `~=` are non-associative. Chaining `a == b == c` is a compile-time error.

---

## 4. Derivation and Algebraic Laws

### 4.1 `~` is an involution
For any type that implements `~`, the implementation **SHOULD** satisfy `~~x == x`. `~` implementations **MUST** be pure and terminating.

### 4.2 Subtraction is definitional
Subtraction is defined as `a - b = a + ~b`. Implementations **MUST NOT** provide independent `-` behavior.

### 4.3 Division is not derived
`/` is a primitive operator. The compiler **MAY** apply algebraic expectations such as `a / b = a * (1/b)` only for types that explicitly opt into field-like semantics (e.g., `Float` under fast-math settings).

---

## 5. Restrictions

### 5.1 No user-defined operator tokens
Programs **MUST NOT** define new operator symbols or precedence levels. Overloading is limited to the built-in operator set.

### 5.2 Reserved symbols
The following are not operators in Zane:

- `!` (reserved for mutating calls; see [`oop.md`](oop.md) §4.2)
- `++`, `--`, `+=`, `-=` and other mutation operators
- `!=` (replaced by `~=` as a derived operator)

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Fixed operator set | Operators are for math; a small stable set keeps code readable and parsing simple. |
| `~` as the only unary operator | Unifies negation and complement without overloading `-`/`!`. |
| Derived `-` and `~=` | Makes mathematical laws inexpressibly exact. |
| `and`/`or` as keywords | Preserves short-circuit control flow. |
| Non-associative equality | Prevents accidental chaining bugs. |
