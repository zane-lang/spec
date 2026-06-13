# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, where operators may be defined, derived operators, precedence, and boolean keywords.

> **See also:** [`syntax.md`](syntax.md) §3.9 and §7 for operator declarations and surface forms. [`effects.md`](effects.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Operators are grammar, not values`.** Operators cannot be referenced as values; they exist only as surface syntax in expressions. Every operator use site carries the operand *values* with their static types, and resolution picks the matching implementation from those types, which is what makes operator overloading unambiguous. See §2.6 below and the parallel rule for function names in [`functions.md`](functions.md) §4.4.
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

### 2.2 Where operators may be defined
Operator implementations are package-scope function declarations whose names are operator tokens. They are ordinary non-`mut` functions with special names, not methods: an operator declaration never has a `this` receiver parameter.

A unary operator is legal only in the home package of its operand type. A binary operator `(left T, right U)` is legal only in the home package of `T` or `U`. See [`functions.md`](functions.md) §6.1 for the home-package concept used by method resolution.

Imported packages do not contribute new implicit operator candidates. This prevents the meaning of `a + b` or `a < b` from changing just because a different helper package was imported.

```zane
Vec2 +(left Int, right Vec2) {
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

### 2.6 Operators are not values
Operators exist only as surface syntax in expressions. There is no surface form that yields an operator as a value, and an operator token cannot appear on the right-hand side of an assignment or in any other value position. The only way an operator is mentioned in source is at a use site, written in its infix or prefix position together with its operands.

This is what makes the operator overloading rules in §2.2 unambiguous. The set of candidate implementations for an operator use site is determined by the static types of the operand *values* at that use site; the operator token itself never carries identity as a name. Because operators are not values, the language has no notion of "the `+` for `Int` and `Float`" as a referenceable thing.

```zane
func Float[Int, Int] = +   // ILLEGAL: operators are not values, only lambda literals can produce a function value
```

The same property extends to function names: a package-scope function is also a grammar token, not a value, so the only way to obtain a function value is a lambda literal or a lambda variable. See [`functions.md`](functions.md) §4.4 for the parallel rule.

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
| 5 | `<` `>` `<=` `>=` `==` `~=` | non-associative |

Comparison operators are non-associative. Chaining `a < b < c` or `a == b == c` is a compile-time error.

### 3.1 Precedence is fixed syntax
Operator precedence is part of the surface grammar. Programs **MUST NOT** declare precedence levels, precedence groups, or type-dependent precedence behavior. Changing operand types may change which implementation is called, but never how the expression groups. Pipe syntax sits immediately below unary `~` in this fixed ordering.

---

## 4. Derivation and Algebraic Laws

### 4.1 `~` is an involution
For any concrete type the call site instantiates the unary `~` operator for, the implementation **SHOULD** satisfy `~~x == x`. `~` implementations **MUST** be pure and terminating.

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

- `!` (reserved for mutating calls; see [`functions.md`](functions.md) §2.5)
- `++`, `--`, `+=`, `-=` and other mutation operators
- `!=` (replaced by `~=` as a derived operator)

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Operators are grammar, not values | Operators are referenced only at their use site, and resolution is driven by the operand *types* at that site, which is what lets operator overloading stay unambiguous. The same property extends to function names in [`functions.md`](functions.md) §4.4. |
| Fixed operator set | Operators are for math; a small stable set keeps code readable and parsing simple. |
| Fixed precedence in syntax | Parsing must be determined by tokens alone so refactors and tooling do not need type information to know grouping. |
| `~` as the only unary operator | Unifies negation and complement without overloading `-`/`!`. |
| `!` reserved for mutating calls | Keeps mutation explicit at method call sites instead of overloading `!` for boolean negation. |
| Home-package operator definitions | Keeps imported helper packages from silently changing operator meaning. |
| Derived `-`, `~=`, `>`, `<=`, and `>=` | Makes law violations inexpressible and keeps comparison surfaces consistent once `<` exists. |
| `/` remains primitive | Multiplicative inverse is not universally available, and `a / b` is not definitionally identical to `a * (1/b)` across all types. |
| `and`/`or` as keywords | Preserves short-circuit control flow. |
| Non-associative equality | Prevents accidental chaining bugs. |
