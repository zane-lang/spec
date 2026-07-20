# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, where operators may be defined, derived operators, precedence, and boolean keywords.

> **See also:** [`syntax.md`](syntax.md) §3.9 and §7 for operator declarations and surface forms. [`effects.md`](effects.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Fixed operator set`.** Operators are not user-defined tokens; only the built-in set exists.
- **`Fixed precedence`.** Grouping is determined by syntax alone and never depends on types or user declarations.
- **`Derived operators`.** Some operators are defined strictly in terms of others and cannot be reimplemented.
- **`Short-circuit logic`.** Boolean `and`/`or` are keywords, not operators, to preserve control flow semantics.

> **Story:** [`stories/operators.md`](../stories/operators.md#a-small-vocabulary-worth-overloading) — "A small vocabulary worth overloading".

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

### 2.2 Where operators may be defined
Operator implementations are package-scope verb declarations whose names are operator tokens. They are ordinary non-`mut` verbs with special names, not methods: an operator declaration never has a `this` receiver parameter.

A unary operator is legal only in the home package of its operand type. A binary operator `(left T, right U)` is legal only in the home package of `T` or `U`. See [`functions.md`](functions.md) §6.1 for the home-package concept used by method resolution.

Imported packages do not contribute new implicit operator candidates. This prevents the meaning of `a + b` or `a < b` from changing just because a different helper package was imported.

```zane
Vec2 +(left Int, right Vec2) {
    ...
}
```

The example above is legal only in the home package of `Int` or `Vec2`.

> **Story:** [`stories/operators.md`](../stories/operators.md#imports-may-add-names-not-meanings) — "Imports may add names, not meanings".

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

> **Story:** [`stories/operators.md`](../stories/operators.md#deriving-the-laws-instead-of-trusting-them) — "Deriving the laws instead of trusting them".

### 2.4 Boolean keywords
`and` and `or` are **keywords**, not operators. They are short-circuiting and therefore not implementable as regular functions.

```zane
if ready and check() { ... }
if ok or fallback() { ... }
```

`or` is defined as a law: `a or b = ~(~a and ~b)`.

> **Story:** [`stories/operators.md`](../stories/operators.md#grouping-is-grammar-all-the-way-down) — "Grouping is grammar all the way down".

### 2.5 Reserved meanings for `!` and `~`
`!` is reserved for mutating method calls and is not boolean NOT in Zane. `~` is the unary complement/flip operator instead:

- `~Bool` is logical complement
- `~Int` / `~Float` are additive inverse
- composite numeric types may define `~` as component-wise negation

Zane does not specify a separate bitwise-complement meaning for `~`.

> **Story:** [`stories/operators.md`](../stories/operators.md#one-operator-for-flipping-a-value) — "One operator for flipping a value".

---

## 3. Precedence and Associativity

`|` pipe syntax is not part of the operator set in §2, but it participates in expression grouping.

A parenthesized expression `(expr)` groups `expr` explicitly. Parentheses bind the enclosed expression as a single unit before the precedence table below is applied to the surrounding syntax.

```zane
number Int = (3 + 2) * 2
```

| Level (high → low) | Syntax / operators | Associativity |
|---|---|---|
| 1 | `~` | — |
| 2 | <code>|</code> pipe syntax | left |
| 3 | `*` `/` | left |
| 4 | `+` `-` | left |
| 5 | `<` `>` `<=` `>=` `==` `~=` | left |

Comparison operators group left. For example, `a < b < c` groups as `(a < b) < c`. The expression is valid only when overload resolution finds an implementation for each grouped operation.

### 3.1 Precedence is fixed syntax
Operator precedence is part of the surface grammar. Programs **MUST NOT** declare precedence levels, precedence groups, or type-dependent precedence behavior. Changing operand types may change which implementation is called, but never how the expression groups. Pipe syntax sits immediately below unary `~` in this fixed ordering.

> **Story:** [`stories/operators.md`](../stories/operators.md#grouping-is-grammar-all-the-way-down) — "Grouping is grammar all the way down".

---

## 4. Derivation and Algebraic Laws

### 4.1 `~` is an involution
For any concrete type the call site instantiates the unary `~` operator for, the implementation **SHOULD** satisfy `~~x == x`. `~` implementations **MUST** be pure and terminating.

### 4.2 Subtraction is definitional
Subtraction is defined as `a - b = a + ~b`. Implementations **MUST NOT** provide independent `-` behavior.

### 4.3 Division is not derived
`/` is a primitive operator. The compiler **MAY** apply algebraic expectations such as `a / b = a * (1/b)` only for types that explicitly opt into field-like semantics (e.g., `Float` under fast-math settings).

> **Story:** [`stories/operators.md`](../stories/operators.md#deriving-the-laws-instead-of-trusting-them) — "Deriving the laws instead of trusting them".

### 4.4 Boolean logic is not regular operator overloading
`and` and `or` cannot be expressed as ordinary overloaded operators because they must short-circuit. A regular operator/function receives already-evaluated arguments, which would destroy the control-flow property that defines boolean conjunction and disjunction.

---

## 5. Restrictions

### 5.1 No user-defined operator tokens
Programs **MUST NOT** define new operator symbols or precedence levels. Overloading is limited to the built-in operator set.

### 5.2 Reserved symbols
The following are not operators in Zane:

- `!` (reserved for mutating calls; see [`functions.md`](functions.md) §2.5)
- `++`, `--`, `+=`, `-=` and other mutation operators
- `!=` (`~=` is the derived inequality operator)

### 5.3 Operators are call-only
An operator token may appear only in operator position; it has no value form. There is no syntax that references `+` or `<` as a value. This is the same rule that makes methods and functions call-only, and it is why an overloaded operator never has to be resolved without operands. To pass behavior as a value, use a lambda-variable.

> **See also:** [`functions.md`](functions.md) §7.1 for the general call-only rule on callables.

> **Story:** [`stories/operators.md`](../stories/operators.md#a-small-vocabulary-worth-overloading) — "A small vocabulary worth overloading".

---

## 6. Summary

| Concept | Rule |
|---|---|
| Operator vocabulary | Only the fixed built-in operator set may be overloaded; programs cannot declare new tokens or precedence. |
| Primitive operators | `~`, `*`, `/`, `+`, `==`, and `<` are independently implementable. |
| Derived operators | `-`, `~=`, `>`, `<=`, and `>=` have fixed desugarings and cannot be implemented independently. |
| Operator definitions | An implementation must live in the home package of at least one operand type. |
| Grouping | Precedence and left associativity are fixed by syntax; parentheses group explicitly. |
| Boolean logic | `and` and `or` are short-circuiting keywords rather than overloadable operators. |
| Callability | Operator tokens are call-only; behavior is passed as a value through a lambda-variable. |
