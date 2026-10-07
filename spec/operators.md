# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, where operators may be defined, derived operators, precedence, Boolean algebra, and the loose forms.

> **See also:** [`syntax.md`](syntax.md) §3.9 and §7 for operator declarations and surface forms. [`effects.md`](effects.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Fixed operator set`.** Operators are not user-defined tokens; only the built-in set exists.
- **`Fixed precedence`.** Grouping is determined by syntax alone and never depends on types or user declarations.
- **`Derived operators`.** Some operators are defined strictly in terms of others and cannot be reimplemented.
- **`Boolean algebra`.** `Bool` draws from the same operator set as every other type: `*` is conjunction, `+` is disjunction, `~` is complement, `==` is equality, and it declares neither `/` nor `<`.
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

A storage primitive's home is the intrinsic namespace that holds it, which is not a package ([`syntax.md`](syntax.md) §2.7). So no operator is declared over storage primitives alone, and a storage primitive has none: an operator is declared on a type of a package, and written over the machine operations of §2.6.

Imported packages do not contribute new implicit operator candidates. This prevents the meaning of `a + b` or `a < b` from changing just because a different helper package was imported.

```zane
Vec2 +(left Int, right Vec2) {
    ...
}
```

Because `Int` is fundamental, the example above is legal only in the home package of `Vec2`.

> **Story:** [`stories/operators.md`](../stories/operators.md#imports-may-add-names-not-meanings) — "Imports may add names, not meanings".
> **Story:** [`stories/operators.md`](../stories/operators.md#literal-operands-find-no-operator-and-the-home-package-stands-in-for-a-qualifier) — "Literal operands find no operator, and the home package stands in for a qualifier".

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

The operands of every operator are evaluated left to right, in written order, and are then passed to the primitive in the positions the desugaring gives them: `f() > g()` evaluates `f()`, then `g()`, and calls `<` with the result of `g()` as its first argument.

> **Story:** [`stories/operators.md`](../stories/operators.md#written-order-survives-the-swap) — "Written order survives the swap".

### 2.4 Boolean operators

`Bool` implements four of the primitive operators of §2.1 — the three of a Boolean algebra, plus equality — and declares nothing beyond them:

| Expression | Meaning |
|---|---|
| `a * b` | conjunction |
| `a + b` | disjunction |
| `~a` | complement |
| `a == b` | equality |

The derived operators of §2.3 that rest on those four follow without separate implementations: `a ~= b` is `~(a == b)`, which on `Bool` is exclusive or, and `a - b` is `a + ~b`, which is `b` implies `a`.

The two primitives `Bool` leaves undeclared are the two a Boolean algebra has no use for. It has no division, so `a / b` on two `Bool` operands is an ordinary no-match error; and it is not ordered, so `a < b` is a no-match error too, along with the `>`, `<=`, and `>=` that §2.3 derives from `<`.

Conjunction and disjunction are interderivable through `~`; see §4.4.

Both operands are evaluated. Conjunction and disjunction are ordinary operator calls (§2.2), so neither skips its right operand. An operand that must run only when the other holds is written inside an `if` on the other, which shows the evaluation at the call site rather than implying it by the token.

```zane
if(ready * check()) { ... }
if(ok + fallback()) { ... }
```

Comparisons are the loosest unprefixed level (§3), so a conjunction of comparisons is written with the loose forms of §3.1 or with brackets:

```zane
if(age > Int(18) '* hasId) { ... }
if((age > Int(18)) * hasId) { ... }
```

> **Story:** [`stories/operators.md`](../stories/operators.md#and-and-or-become--and-) — "`and` and `or` become `*` and `+`".

### 2.5 Reserved meanings for `!` and `~`

`!` is reserved for mutating method calls and is not boolean NOT in Zane. `~` is the unary complement/flip operator instead:

- `~Bool` is logical complement
- `~Int` / `~Float` are additive inverse
- composite numeric types may define `~` as component-wise negation

Zane does not specify a separate bitwise-complement meaning for `~`.

> **Story:** [`stories/operators.md`](../stories/operators.md#one-operator-for-flipping-a-value) — "One operator for flipping a value".

### 2.6 The machine operations operators are written over

The **machine operations** on storage primitives are the functions of `@operators$` ([`syntax.md`](syntax.md) §2.7). They are called like any function. Each arithmetic operation and comparison has one overload per scalar primitive, `@primitives$I32`, `@primitives$I64`, `@primitives$F32`, and `@primitives$F64`, and takes both operands of that one type:

| Operation | Signature |
|---|---|
| `add` | `S @operators$add(left S, right S)` |
| `multiply` | `S @operators$multiply(left S, right S)` |
| `divide` | `S @operators$divide(left S, right S)` |
| `negate` | `S @operators$negate(value S)` |
| `equal` | `@primitives$Bool @operators$equal(left S, right S)` |
| `lessThan` | `@primitives$Bool @operators$lessThan(left S, right S)` |

On `@primitives$I32` and `@primitives$I64`, an arithmetic operation whose exact result the type cannot hold wraps: the result is the exact one reduced to the type's width in two's complement, so `negate` and `divide` of the most negative value by `-1` both give that value back. `divide` rounds its quotient toward zero, and a division by zero gives zero, so every integer operation has a result.

On `@primitives$F32` and `@primitives$F64`, each operation is IEEE 754's: `add`, `multiply`, and `divide` are its addition, multiplication, and division, rounded to nearest with ties to even as a conversion is ([`types.md`](types.md) §2.9), and `negate` flips the sign, of a zero and a NaN too. So an operation on infinities gives what IEEE 754 gives, an infinity or a NaN, a division of a nonzero value by zero gives an infinity of the quotient's sign, zero divided by zero gives a NaN, and an operation with a NaN operand gives a NaN. `equal` and `lessThan` are IEEE 754's quiet comparisons: both are false when either operand is a NaN, so a NaN equals nothing, itself included, and `equal` holds of a positive and a negative zero.

`@primitives$Bool` and `@primitives$String` have machine operations of their own:

| Operation | Signature |
|---|---|
| `and` | `@primitives$Bool @operators$and(left @primitives$Bool, right @primitives$Bool)` |
| `or` | `@primitives$Bool @operators$or(left @primitives$Bool, right @primitives$Bool)` |
| `not` | `@primitives$Bool @operators$not(value @primitives$Bool)` |
| `concat` | `@primitives$String @operators$concat(left @primitives$String, right @primitives$String)` |
| `equal` | `@primitives$Bool @operators$equal(left B, right B)`, for `B` either of the two |

`concat` gives a new string holding the left operand's bytes followed by the right's. `equal` on `@primitives$String` compares contents: two strings are equal when they hold the same bytes in the same order, whatever storage holds them.

No machine operation takes operands of two types; an operand is converted first ([`types.md`](types.md) §2.9). There is no subtraction, since `a - b` is `a + ~b` (§4.2).

`core` writes the fundamental types' operators over these machine operations. Where its `Int` holds an `@primitives$I64` in a field `value`, and its `Bool` an `@primitives$Bool`:

```zane
Int +(left Int, right Int) => Int(@operators$add(left.value, right.value))
Bool ~(value Bool) => Bool(@operators$not(value.value))
```

> **Story:** [`stories/operators.md`](../stories/operators.md#primitives-lose-their-operators-to-operators-functions) — "Primitives lose their operators to `@operators$` functions".

---

## 3. Precedence and Associativity

A parenthesized expression `(expr)` groups `expr` explicitly. Parentheses bind the enclosed expression as a single unit before the precedence table below is applied to the surrounding syntax.

```zane
number Int = (3 + 2) * 2;
```

| Level (high → low) | Syntax / operators | Associativity |
|---|---|---|
| 1 | `~` | — |
| 2 | `*` `/` | left |
| 3 | `+` `-` | left |
| 4 | `<` `>` `<=` `>=` `==` `~=` | left |
| 5 | `'*` `'/` | left |
| 6 | `'+` `'-` | left |
| 7 | `'<` `'>` `'<=` `'>=` `'==` `'~=` | left |

Comparison operators group left. For example, `a < b < c` groups as `(a < b) < c`. The expression is valid only when overload resolution finds an implementation for each grouped operation.

### 3.1 A `'` prefix selects the loose form of a binary operator

Levels 5 through 7 are a **mirror** of levels 2 through 4: the same binary operators, in the same relative order, written with a leading `'`. A loose operator calls the same implementation as its unprefixed form and differs only in where it groups.

```zane
ready Bool = age > Int(18) '* hasId;      // (age > 18) * hasId
band Bool = a == b '* c == d '+ e == f;   // ((a == b) * (c == d)) + (e == f)
```

The mirror is one tier deep. A second prefix is not a further shift:

```zane
a ''* b    // ILLEGAL: there is no second loose tier
```

Only binary operators have a loose form. Unary `~` binds tightest and has nothing to separate itself from, so it has none:

```zane
'~a        // ILLEGAL: unary operators have no loose form
```

The loose forms are surface grammar like every other level. They add no token to the operator vocabulary (§5.1) and no way for a program to place an operator at a level of its choosing: which level a loose operator occupies is fixed by the mirror, exactly as the unprefixed level is fixed by the table.

> **See also:** [`lexical.md`](lexical.md) §4.3 for `'` as a reserved sigil.

> **Story:** [`stories/operators.md`](../stories/operators.md#the-loose-operator-forms-and-the--prefix) — "The loose operator forms and the `'` prefix".

### 3.2 Precedence is fixed syntax

Operator precedence is part of the surface grammar. Programs **MUST NOT** declare precedence levels, precedence groups, or type-dependent precedence behavior. Changing operand types may change which implementation is called, but never how the expression groups. The loose forms of §3.1 occupy fixed levels of their own beneath every unprefixed one.

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

> **Story:** [`stories/operators.md`](../stories/operators.md#and-and-or-become--and-) — "`and` and `or` become `*` and `+`".

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
| Operand order | The operands of every operator are evaluated left to right, in written order, whatever position the desugaring passes them in. |
| Operator definitions | An implementation must live in the home package of at least one operand type; operators over the fundamental types alone live in `core`, and a storage primitive has none. |
| Machine operations | `@operators$` supplies `add`, `multiply`, `divide`, `negate`, `equal`, and `lessThan` on each scalar primitive, `and`, `or`, `not`, and `equal` on `@primitives$Bool`, and `concat` and `equal` on `@primitives$String`; each takes operands of one type. |
| Grouping | Precedence and left associativity are fixed by syntax; parentheses group explicitly. |
| Boolean logic | `Bool` implements `*` as conjunction, `+` as disjunction, `~` as complement, and `==` as equality; `~=` is exclusive or and `-` is implication by derivation. It declares no `/` and no `<`, so those and the operators derived from `<` are no-match errors. Both operands are evaluated. |
| Loose forms | A `'` prefix selects a binary operator at a mirrored level below every unprefixed one; one tier only, binary only, same implementation. |
| Callability | Operator tokens are call-only; behavior is passed as a value through a lambda-variable. |
