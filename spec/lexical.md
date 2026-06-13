# Zane Lexical Rules

This document specifies Zane's lexical layer: how source text is tokenised, how identifiers are formed, and how casing is significant. Zane parses case-sensitively, and the casing of an identifier's first letter decides whether it names a type or a value. These rules are what let the rest of the grammar — in particular the `<>` type syntax — stay unambiguous.

> **See also:** [`syntax.md`](syntax.md) for the surface grammar built on these tokens. [`generics.md`](generics.md) §4.3 for how casing disambiguates `<>` type expressions. [`types.md`](types.md) §5 for `type` and `alias` declarations.

---

## 1. Overview

Zane is case-sensitive, and casing is load-bearing rather than stylistic. The first letter of a name selects its lexical class, so the same spelling in two cases is two different — and differently-classed — identifiers.

- **`Case-sensitive`.** `Vector` and `vector` are distinct identifiers. Casing is never normalised.
- **`Casing determines kind`.** An uppercase-initial name is a type; a lowercase-initial name is a value, binding, or parameter. Writing one where the casing implies the other is a compile-time error.
- **`Digits are ordinary identifier characters`.** A digit may appear in a name except as its first character, so names such as `Vec2` and `Tensor3` are ordinary names.
- **`Casing keeps the grammar unambiguous`.** Because only a type may precede `<` in a type expression, the parser tells `Vector<Int>` from `a < b` by casing alone.

---

## 2. Case Sensitivity

Identifiers are compared by exact byte sequence. Two names that differ only in case are two distinct identifiers and never refer to the same entity.

```zane
Vector   // a type
vector   // a value-level name
```

The compiler never folds case. There is no implicit conversion between a name and its differently-cased spelling.

---

## 3. Casing Determines Kind

The first letter of an identifier selects its lexical class.

| First letter | Class | Examples |
|---|---|---|
| Uppercase | Type | `Int`, `Vector`, `Vec2`, `Matrix` |
| Lowercase | Value, binding, or parameter | `x`, `count`, `n`, `transform` |

### 3.1 Types must be uppercase

A name used in a type position **MUST** be uppercase-initial. A lowercase name in a type position is a compile-time error.

```zane
vec Vector(Int(2), Int(3))   // legal: Vector and Int are types
vec vector(int(2))           // ILLEGAL: lowercase names are not types
```

### 3.2 Values must be lowercase

A binding, parameter, or field name is lowercase-initial. This is why a number parameter such as `n` is known to be a number and not a type: its casing places it in the value class even when it appears inside a `<>` slot.

```zane
type Buffer<'T, n> = struct {   // 'T is a type parameter, n is a number parameter
    data Array<'T, n>
}
```

A type parameter is the one value-of-the-type-system case: it is written with a leading `'` and an uppercase letter (`'T`), which marks it as a stand-in for a type rather than a concrete type. See [`generics.md`](generics.md) §3.

---

## 4. Identifiers

### 4.1 Identifier characters

An identifier is a letter followed by zero or more letters or digits. A digit **MUST NOT** be the first character.

```zane
Vec2       // legal
Tensor3    // legal
2Vec       // ILLEGAL: an identifier cannot start with a digit
```

Digits carry no special meaning inside a name; `Vec2` is an ordinary type name, not a parameterised form. Parameters are supplied only through `<>` (see [`generics.md`](generics.md) §4).

### 4.2 Leading underscore

A leading `_` marks a field as private to methods whose first parameter is `this` for that type. The underscore does not change the lexical class set by the first letter. See [`types.md`](types.md) §2.3.

### 4.3 Reserved sigils

Certain leading characters are reserved and are not ordinary identifier starts:

| Sigil | Meaning | Canonical home |
|---|---|---|
| `'` | Type-parameter marker (`'T`) | [`generics.md`](generics.md) §3 |
| `&` | Reference type (`&Node`) | [`memory.md`](memory.md) §2 |
| `@` | Reserved compiler namespace (`@primitives$`, `@concepts$`) | [`syntax.md`](syntax.md) §2.7 |
| `$` | Package-member separator (`PackageName$member`) | [`packages.md`](packages.md) §1 |

---

## 5. How Casing Disambiguates the Grammar

The `<>` type-expression syntax shares its characters with the `<` and `>` comparison operators. Casing resolves the overlap without lookahead: the operand on the left of `<` in a type expression is always a type, and a type is always uppercase-initial.

```zane
data Array<Int, 9>   // type expression: Array is uppercase, so < opens a type argument list
ok Bool = a < b      // comparison: a is lowercase, so < is the comparison operator
```

A comparison never has a type on its immediate left, and a type expression never has a value on its immediate left. The casing rule therefore tells the two apart by inspecting a single token's first letter.

> **See also:** [`generics.md`](generics.md) §4.3 for the type-expression rule and [`operators.md`](operators.md) for comparison-operator semantics.

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Case-sensitive parsing | Casing can carry meaning only if it is never normalised. Two cases of one spelling being one identifier would erase the kind distinction below. |
| Casing determines kind | Encoding type-vs-value in the first letter removes a class of ambiguity at the lexical layer, so later grammar rules do not need declarations or lookahead to know what a bare name is. |
| Lowercase types are an error | A hard error, rather than a warning or a convention, is what lets the parser and reader rely on casing as ground truth everywhere. |
| Digits allowed inside names | Names such as `Vec2` and `Matrix3` read naturally, and with parameters supplied only through `<>` there is no need to reserve digits for a baked-in size form. |
| Casing disambiguates `<>` | Reusing `<` and `>` for type arguments is only safe because the left operand's casing distinguishes a type application from a comparison. |

---

## 7. Summary

| Concept | Rule |
|---|---|
| Case sensitivity | Identifiers compare by exact bytes; case is never folded |
| Type names | Uppercase-initial; a lowercase name in a type position is a compile-time error |
| Value names | Lowercase-initial; bindings, parameters, and fields |
| Number parameter | A lowercase name in a `<>` slot is a number, not a type |
| Type parameter | A leading `'` with an uppercase letter (`'T`) marks a type stand-in |
| Digits | Legal in a name except as the first character; carry no special meaning |
| `<>` disambiguation | A type (uppercase) on the left means a type argument list; a value (lowercase) means comparison |
