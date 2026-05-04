# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, where operators may be defined, derived operators, precedence, and boolean keywords.

> **See also:** [`syntax.md`](syntax.md) §3.8 and §6 for operator declarations and surface forms. [`purity.md`](purity.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Fixed operator set`.** Operators are not user-defined tokens; only the built-in set exists.
- **`Fixed precedence`.** Grouping is determined by syntax alone and never depends on types or user declarations.
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
| `<` | binary | `Bool <(left T, right T)` |

### 2.2 Definition site
Operator implementations are package-scope declarations whose names are operator tokens. A unary operator is legal only in the home package of its operand type. A binary operator `(left T, right U)` is legal only in the home package of `T` or `U`.

Imported packages do not contribute new implicit operator candidates. This prevents the meaning of `a + b` or `a < b` from changing just because a different helper package was imported.

```zane
Vec2 +(first Int, second Vec2) {
    ...
}
```

The example above is legal only in the home package of `Int` or `Vec2`.

### 2.3 Derived operators
Derived operators are fixed desugarings and are **not** independently implementable:

| Operator | Desugars to |
|---|---|
| `a - b` | `a + ~b` |
| `a ~= b` | `~(a == b)` |
| `a > b` | `b < a` |
| `a <= b` | `~(b < a)` |
| `a >= b` | `~(a < b)` |

If a type provides `<` for an operand pair, users automatically get `>`, `<=`, and `>=` for that same pair.

### 2.4 Boolean keywords
`and` and `or` are **keywords**, not operators. They are short-circuiting and therefore not implementable as regular functions.

```zane
if ready and check() { ... }
if ok or fallback() { ... }
```

`or` is defined as a law: `a or b = ~(~a and ~b)`.

### 2.5 Reserved meanings for `!` and `~`
`!` is reserved for mutating method calls and is not boolean NOT in Zane. `~` is the unary complement/flip operator instead:

- `~Bool` is logical complement
- `~Int` / `~Float` are additive inverse
- composite numeric types may define `~` as component-wise negation

Zane does not specify a separate bitwise-complement meaning for `~`.

---

## 3. Precedence and Associativity

| Level (high → low) | Operators | Associativity |
|---|---|---|
| 1 | `~` | — |
| 2 | `*` `/` | left |
| 3 | `+` `-` | left |
| 4 | `<` `>` `<=` `>=` `==` `~=` | non-associative |

Comparison operators are non-associative. Chaining `a < b < c` or `a == b == c` is a compile-time error.

### 3.1 Precedence is fixed syntax
Operator precedence is part of the surface grammar. Programs **MUST NOT** declare precedence levels, precedence groups, or type-dependent precedence behavior. Changing operand types may change which implementation is called, but never how the expression groups.

---

## 4. Derivation and Algebraic Laws

### 4.1 `~` is an involution
For any type that implements `~`, the implementation **SHOULD** satisfy `~~x == x`. `~` implementations **MUST** be pure and terminating.

### 4.2 Subtraction is definitional
Subtraction is defined as `a - b = a + ~b`. Implementations **MUST NOT** provide independent `-` behavior.

### 4.3 Division is not derived
`/` is a primitive operator. The compiler **MAY** apply algebraic expectations such as `a / b = a * (1/b)` only for types that explicitly opt into field-like semantics (e.g., `Float` under fast-math settings).

### 4.4 Boolean logic is not regular operator overloading
`and` and `or` cannot be expressed as ordinary overloaded operators because they must short-circuit. A regular operator/function receives already-evaluated arguments, which would destroy the control-flow property that defines boolean conjunction and disjunction.

---

## 5. Restrictions

### 5.1 No user-defined operator tokens
Programs **MUST NOT** define new operator symbols or precedence levels. Overloading is limited to the built-in operator set.

### 5.2 Reserved symbols
The following are not operators in Zane:

- `!` (reserved for mutating calls; see [`oop.md`](oop.md) §4.2)
- `++`, `--`, `+=`, `-=` and other mutation operators
- `!=` (replaced by `~=` as a derived operator)

### 5.3 No out-of-package operator definitions
Programs **MUST NOT** declare an operator in a package unrelated to its operand types. If neither operand's home package is the current package, the declaration is illegal.

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Fixed operator set | Operators are for math; a small stable set keeps code readable and parsing simple. |
| Fixed precedence in syntax | Parsing must be determined by tokens alone so refactors and tooling do not need type information to know grouping. |
| `~` as the only unary operator | Unifies negation and complement without overloading `-`/`!`. |
| `!` reserved for mutating calls | Keeps mutation explicit at method call sites instead of overloading `!` for boolean negation. |
| Home-package operator definitions | Keeps imported helper packages from silently changing operator meaning. |
| Derived `-`, `~=`, `>`, `<=`, and `>=` | Makes law violations inexpressible and keeps comparison surfaces consistent once `<` exists. |
| `/` remains primitive | Multiplicative inverse is not universally available, and `a / b` is not definitionally identical to `a * (1/b)` across all types. |
| `and`/`or` as keywords | Preserves short-circuit control flow. |
| Non-associative equality | Prevents accidental chaining bugs. |
