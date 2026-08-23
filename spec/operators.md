# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, where operators may be defined, derived operators, precedence, and boolean keywords.

> **See also:** [`syntax.md`](syntax.md) §3.9 and §7 for operator declarations and surface forms. [`effects.md`](effects.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Fixed operator set`.** Operators are not user-defined tokens; only the built-in set exists.
- **`Fixed precedence`.** Grouping is determined by syntax alone and never depends on types or user declarations.
- **`Derived operators`.** Some operators are defined strictly in terms of others and cannot be reimplemented.
- **`Boolean algebra`.** `Bool` uses the same operator set as every other type: `*` is conjunction, `+` is disjunction, `~` is complement.
- **`Loose forms`.** A `'` prefix moves a binary operator into a mirror of the precedence table below the unprefixed one, so a chain of comparisons can be combined without brackets.

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
Operator implementations are package-scope verb declarations whose names are operator tokens. They are ordinary non-`mut` verbs with special names, not methods: an operator declaration never has a `this` subject parameter.

A unary operator is legal only in the home package of its operand type. A binary operator `(left T, right U)` is legal only in the home package of `T` or `U`. `core` is the home package of the fundamental types, and a package may no more add declarations to it than to any other package it does not own; a fundamental operand therefore does not by itself grant permission to declare an operator. See [`functions.md`](functions.md) §6.1 for the corresponding method-resolution rule.

Imported packages do not contribute new implicit operator candidates. This prevents the meaning of `a + b` or `a < b` from changing just because a different helper package was imported.

```zane
Vec2 +(left Int, right Vec2) {
    ...
}
```

Because `Int` is fundamental, the example above is legal only in the home package of `Vec2`.

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

### 2.4 Boolean operators
`Bool` implements the primitive operator set of §2.1 as a Boolean algebra and declares nothing beyond it:

| Expression | Meaning |
|---|---|
| `a * b` | conjunction |
| `a + b` | disjunction |
| `~a` | complement |

The derived operators of §2.3 follow without separate implementations. `a ~= b` is `~(a == b)`, which on `Bool` is exclusive or; `a - b` is `a + ~b`, which is `b` implies `a`. `Bool` implements no `/`, so `a / b` on two `Bool` operands is an ordinary no-match error.

Conjunction and disjunction are interderivable through `~`; see §4.4.

Both operands are evaluated. Conjunction and disjunction are ordinary operator calls (§2.2), so neither skips its right operand. A type that wants a deferred right operand declares an overload taking one; the evaluation behaviour is then visible at the call site rather than implied by the token.

```zane
if(ready * check()) { ... }
if(ok + fallback()) { ... }
```

Comparisons are the loosest unprefixed level (§3), so a conjunction of comparisons is written with the loose forms of §3.1 or with brackets:

```zane
if(age > Int(18) '* hasId) { ... }
if((age > Int(18)) * hasId) { ... }
```

> **Story:** [`stories/operators.md`](../stories/operators.md#the-keyword-that-was-neither) — "The keyword that was neither".

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
| 6 | `'*` `'/` | left |
| 7 | `'+` `'-` | left |
| 8 | `'<` `'>` `'<=` `'>=` `'==` `'~=` | left |

Comparison operators group left. For example, `a < b < c` groups as `(a < b) < c`. The expression is valid only when overload resolution finds an implementation for each grouped operation.

### 3.1 A `'` prefix selects the loose form of a binary operator

Levels 6 through 8 are a **mirror** of levels 3 through 5: the same binary operators, in the same relative order, written with a leading `'`. A loose operator calls the same implementation as its unprefixed form and differs only in where it groups.

```zane
ready Bool = age > Int(18) '* hasId       // (age > 18) * hasId
band Bool = a == b '* c == d '+ e == f    // ((a == b) * (c == d)) + (e == f)
```

The mirror is one tier deep. A second prefix is not a further shift:

```zane
a ''* b    // ILLEGAL: there is no second loose tier
```

Only binary operators have a loose form. Unary `~` binds tightest and has nothing to separate itself from, and `|` pipe syntax is not part of the operator set of §2, so neither has one:

```zane
'~a        // ILLEGAL: unary operators have no loose form
a '| f()   // ILLEGAL: pipe syntax has no loose form
```

The loose forms are surface grammar like every other level. They add no token to the operator vocabulary (§5.1) and no way for a program to place an operator at a level of its choosing: which level a loose operator occupies is fixed by the mirror, exactly as the unprefixed level is fixed by the table.

> **See also:** [`lexical.md`](lexical.md) §4.3 for `'` as a reserved sigil.

> **Story:** [`stories/operators.md`](../stories/operators.md#a-tier-below-everything) — "A tier below everything".

### 3.2 Precedence is fixed syntax
Operator precedence is part of the surface grammar. Programs **MUST NOT** declare precedence levels, precedence groups, or type-dependent precedence behavior. Changing operand types may change which implementation is called, but never how the expression groups. Pipe syntax sits immediately below unary `~` in this fixed ordering, and the loose forms of §3.1 occupy fixed levels of their own beneath every unprefixed one.

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

### 4.4 Conjunction and disjunction are interderivable
For a type whose `~` is a complement, either binary operator derives the other:

```zane
a + b == ~(~a * ~b)
~(a * b) == ~a + ~b
```

`~Int` and `~Float` are additive inverses rather than complements (§2.5), so these identities do not hold there and the logical reading of `*` and `+` is available only to `Bool` and to user-defined types complemented under their own `~`.

> **Story:** [`stories/operators.md`](../stories/operators.md#the-keyword-that-was-neither) — "The keyword that was neither".

---

## 5. Restrictions

### 5.1 No user-defined operator tokens
Programs **MUST NOT** define new operator symbols or precedence levels. Overloading is limited to the built-in operator set. The loose forms of §3.1 are part of that fixed set rather than an exception to it: `'` selects an existing operator at a fixed level, and no program can introduce a token or place one at a level of its own choosing.

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
| Operator definitions | An implementation must live in the home package of at least one operand type; operators over the fundamental types alone live in `core`. |
| Grouping | Precedence and left associativity are fixed by syntax; parentheses group explicitly. |
| Boolean logic | `Bool` implements `*` as conjunction, `+` as disjunction, `~` as complement; `~=` is exclusive or and `-` is implication by derivation. Both operands are evaluated. |
| Loose forms | A `'` prefix selects a binary operator at a mirrored level below every unprefixed one; one tier only, binary only, same implementation. |
| Callability | Operator tokens are call-only; behavior is passed as a value through a lambda-variable. |
