# Zane Operator System

This document specifies Zane's operator system: the fixed operator set, where operators may be defined, derived operators, intersection and union, how operands are matched, and precedence.

> **See also:** [`syntax.md`](syntax.md) §3.9 and §7 for operator declarations and surface forms. [`effects.md`](effects.md) §2 for `mut` and side effects.

---

## 1. Overview

Zane treats operators as mathematical notation with a small, fixed vocabulary.

- **`Fixed operator set`.** Operators are not user-defined tokens; only the built-in set exists.
- **`Fixed precedence`.** Grouping is determined by syntax alone and never depends on types or user declarations.
- **`Derived operators`.** Some operators are defined strictly in terms of others and cannot be reimplemented.
- **`Intersection and union`.** `and` and `or` take what two values share and what either covers. `Bool` reads them as conjunction and disjunction, and they bind below every comparison.
- **`Operands as written`.** An operand is never converted. An overload states each type it accepts, a literal's concept type included.

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
| `and` | binary | `T and(left T, right T)` |
| `or` | binary | `T or(left T, right T)` |

`and` and `or` are words, reserved as operator tokens ([`lexical.md`](lexical.md) §4.4).

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

### 2.4 Intersection and union

`and` and `or` are the operators of **intersection** and **union**. `a and b` is what `a` and `b` share, and `a or b` is what either of them covers. A type implements them when its values have parts in common in that sense:

| Type | `a and b` | `a or b` |
|---|---|---|
| `Bool` | conjunction | disjunction |
| a set | the elements in both | the elements in either |
| a set of flags | the flags both hold | the flags held by either |

A number has no parts of that kind, so `Int` and `Float` implement neither.

An implementation's operand and result types follow §2.2 like those of any other operator. A package may declare `or` over two members of its own enum and return a set of them:

```zane
type Permission = enum [ read, write, execute ];

Permissions or(left Permission, right Permission) { ... }

access Permissions = Permission.read or Permission.write;
```

`Bool` implements `and`, `or`, `~` as complement, and `==`, and declares nothing beyond them. `a ~= b` follows from §2.3 and is exclusive or on `Bool`. `Bool` declares no `*`, `/`, `+`, or `<`, so those operators and the ones §2.3 derives from them are no-match errors on two `Bool` operands.

Both operands are evaluated. `and` and `or` are ordinary operator calls (§2.2), so neither skips its right operand. An operand that must run only when it decides the result is written inside a branch on the other: the right operand of a conjunction inside an `if` on the left, and the right operand of a disjunction inside the `else` of one. The evaluation then shows at the call site.

`and` and `or` bind below every comparison (§3), so a combination of comparisons needs no brackets:

```zane
if(ready and check()) { ... }
if(ok or fallback()) { ... }
if(age > 18 and hasId) { ... }
```

For a type whose `~` is a complement, each of the two derives the other; see §4.4.

> **Story:** [`stories/operators.md`](../stories/operators.md#and-and-or-return-as-intersection-and-union-and-the-loose-forms-go) — "`and` and `or` return as intersection and union, and the loose forms go".
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
| `conjoin` | `@primitives$Bool @operators$conjoin(left @primitives$Bool, right @primitives$Bool)` |
| `disjoin` | `@primitives$Bool @operators$disjoin(left @primitives$Bool, right @primitives$Bool)` |
| `not` | `@primitives$Bool @operators$not(value @primitives$Bool)` |
| `concat` | `@primitives$String @operators$concat(left @primitives$String, right @primitives$String)` |
| `equal` | `@primitives$Bool @operators$equal(left B, right B)`, for `B` either of the two |

`conjoin` and `disjoin` are conjunction and disjunction. `concat` gives a new string holding the left operand's bytes followed by the right's. `equal` on `@primitives$String` compares contents: two strings are equal when they hold the same bytes in the same order, whatever storage holds them.

No machine operation takes operands of two types; an operand is converted first ([`types.md`](types.md) §2.9). There is no subtraction, since `a - b` is `a + ~b` (§4.2).

`core` writes the fundamental types' operators over these machine operations. Where its `Int` holds an `@primitives$I64` in a field `value`, and its `Bool` an `@primitives$Bool`:

```zane
Int +(left Int, right Int) => Int(@operators$add(left.value, right.value));
Bool ~(value Bool) => Bool(@operators$not(value.value));
```

> **Story:** [`stories/operators.md`](../stories/operators.md#primitives-lose-their-operators-to-operators-functions) — "Primitives lose their operators to `@operators$` functions".

### 2.7 Operands are passed as written

An operator converts no operand. Implicit constructors ([`types.md`](types.md) §4) are never inserted at an operand, so an overload is viable only when the type of each operand is the type of its parameter, directly or through generic inference ([`functions.md`](functions.md) §5).

A literal operand has a compiler concept type ([`syntax.md`](syntax.md) §2.8). An overload accepts it by declaring that concept type as its parameter, and builds the value it needs inside its body. `core` declares one for each side of each operator over a fundamental type:

```zane
Int +(left Int, right @concepts$Int) => left + Int(right);
Int +(left @concepts$Int, right Int) => Int(left) + right;
```

So `count + 1` and `1 + count` both resolve directly.

`~` on an integer or float literal calls no operator. The result is the literal of the opposite sign, of the same concept type, so `~1` is an integer literal and `~2.5` a float literal. A literal parameter stands for its literal, and `~` flips it the same way. A derived subtraction with a literal on the right therefore resolves like an addition: `count - 1` is `count + ~1` (§2.3), which `core`'s overload above takes.

```zane
less Int = count - 1;        // count + ~1
below Int = Int(~1);         // a negative literal, constructed
```

Operands of two different types reach an overload only when one declares that pair:

```zane
total Float = count + ratio;           // ILLEGAL: no `+` takes an Int and a Float
total Float = Float(count) + ratio;    // legal: the conversion is written
```

A concept type belongs to no package, so two literal operands find no candidate under §2.2. One of them is written as a value of the type it stands for:

```zane
area Float = 3.0 * 3.0;           // ILLEGAL: no operand names a home package
area Float = Float(3.0) * 3.0;    // legal
```

> **Story:** [`stories/operators.md`](../stories/operators.md#operators-never-coerce-and-a-literal-operand-has-an-overload-of-its-own) — "Operators never coerce, and a literal operand has an overload of its own".

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
| 5 | `and` | left |
| 6 | `or` | left |

Comparison operators group left. For example, `a < b < c` groups as `(a < b) < c`. The expression is valid only when overload resolution finds an implementation for each grouped operation.

`and` and `or` sit below every comparison, and `and` binds tighter than `or`:

```zane
band Bool = a == b and c == d or e == f;   // ((a == b) and (c == d)) or (e == f)
```

> **Story:** [`stories/operators.md`](../stories/operators.md#and-and-or-return-as-intersection-and-union-and-the-loose-forms-go) — "`and` and `or` return as intersection and union, and the loose forms go".

### 3.1 Precedence is fixed syntax

Operator precedence is part of the surface grammar. Programs **MUST NOT** declare precedence levels, precedence groups, or type-dependent precedence behavior. Changing operand types may change which implementation is called, but never how the expression groups.

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

### 4.4 Laws of intersection and union

Where `and` and `or` take two operands of one type and return that type, the implementations **SHOULD** satisfy the laws of intersection and union:

```zane
(a and b) == (b and a)                  // commutative, and likewise for `or`
((a and b) and c) == (a and (b and c))  // associative, and likewise for `or`
(a and a) == a                          // idempotent, and likewise for `or`
(a and (a or b)) == a                   // absorptive
(a or (a and b)) == a
```

For a type whose `~` is a complement, as on `Bool`, either operator derives the other:

```zane
(a or b) == ~(~a and ~b)
~(a and b) == (~a or ~b)
```

> **Story:** [`stories/operators.md`](../stories/operators.md#and-and-or-return-as-intersection-and-union-and-the-loose-forms-go) — "`and` and `or` return as intersection and union, and the loose forms go".

---

## 5. Restrictions

### 5.1 No user-defined operator tokens

Programs **MUST NOT** define new operator symbols or precedence levels. Overloading is limited to the built-in operator set, whose two words `and` and `or` are as fixed as its symbols.

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
| Primitive operators | `~`, `*`, `/`, `+`, `==`, `<`, `and`, and `or` are independently implementable. |
| Derived operators | `-`, `~=`, `>`, `<=`, and `>=` have fixed desugarings and cannot be implemented independently. |
| Operand order | The operands of every operator are evaluated left to right, in written order, whatever position the desugaring passes them in. |
| Operator definitions | An implementation must live in the home package of at least one operand type; operators over the fundamental types alone live in `core`, and a storage primitive has none. |
| Machine operations | `@operators$` supplies `add`, `multiply`, `divide`, `negate`, `equal`, and `lessThan` on each scalar primitive, `conjoin`, `disjoin`, `not`, and `equal` on `@primitives$Bool`, and `concat` and `equal` on `@primitives$String`; each takes operands of one type. |
| Grouping | Precedence and left associativity are fixed by syntax; parentheses group explicitly. |
| Intersection and union | `and` and `or` take what two values share and what either covers, binding below every comparison with `and` tighter; `Bool` reads them as conjunction and disjunction, and the numeric types implement neither. Both operands are evaluated. |
| Operands | An operand is never converted; an overload accepts a literal by declaring its concept type, and `core` declares one for each side of each operator over a fundamental type. `~` on a numeric literal is the literal of the opposite sign. |
| Callability | Operator tokens are call-only; behavior is passed as a value through a lambda-variable. |
