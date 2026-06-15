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
- **`Delimiter follows the separated thing`.** `;` terminates every member of a `struct`/`class`/`variant` body (always trailing), `,` separates elements of a value collection (never trailing), and a newline separates statements. `{ }` holds a member body or code block; `[ ]` holds a flat list.

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
| Lowercase | Package name | `math`, `json`, `httpClient` |

Package names share lowercase-initial casing with value names but are syntactically distinct: they appear only after the `package` keyword, after `import`, or as the left operand of `$`. The surrounding syntax, not casing, disambiguates them from ordinary value names.

### 3.1 Types must be uppercase

A name used in a type position **MUST** be uppercase-initial. A lowercase name in a type position is a compile-time error.

```zane
vec Vector(Int(2), Int(3))   // legal: Vector and Int are types
vec vector(int(2))           // ILLEGAL: lowercase names are not types
```

### 3.2 Values must be lowercase

A binding, parameter, or field name is lowercase-initial. This is why a number parameter such as `n` is known to be a number and not a type: its casing places it in the value class even when it appears inside a `<>` slot.

```zane
type Buffer<T Type, n Number> = struct {   // T is a type parameter, n is a number parameter
    data Array<T, n>;
}
```

A type parameter such as `T` is uppercase because it names a type; a number parameter such as `n` is lowercase because it names a compile-time number. A parameter is introduced by the `<>` header and referenced by its bare name. See [`generics.md`](generics.md) §3.

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
| `&` | Reference type (`&Node`) | [`memory.md`](memory.md) §2 |
| `@` | Reserved compiler namespace (`@primitives$`, `@concepts$`) | [`syntax.md`](syntax.md) §2.7 |
| `$` | Package-member separator (`packageName$member`) | [`packages.md`](packages.md) §1 |

---

## 5. How Casing Disambiguates the Grammar

The `<>` type-expression syntax shares its characters with the `<` and `>` comparison operators. Casing resolves the overlap without lookahead: the operand on the left of `<` in a type expression is always a type, and a type is always uppercase-initial.

```zane
type Holder = struct {
    data Array<Int, 9>;   // type expression: Array is uppercase, so < opens a type argument list
}

ok Bool = a < b          // comparison: a is lowercase, so < is the comparison operator
```

A comparison never has a type on its immediate left, and a type expression never has a value on its immediate left. The casing rule therefore tells the two apart by inspecting a single token's first letter.

> **See also:** [`generics.md`](generics.md) §4.3 for the type-expression rule and [`operators.md`](operators.md) for comparison-operator semantics.

---

## 6. Delimiters and Brackets

Zane chooses its delimiter by *what is being separated*, and its bracket by *what kind of thing is inside*. The choice is mechanical, so the same character never means two things in one context.

### 6.1 `;` terminates members of a declaration body

A `;` **terminates** every member of a `struct`, `class`, or `variant` type-definition body. It is **always trailing**: every member ends with a `;`, inline or multiline, single-member or many, because newlines are **insignificant inside these three bodies**. The last member carries a `;` exactly like every other, so the form is uniform.

```zane
type Node = class {
    _id Int;
    scale Float;
    label String;
}

type Color = struct { r Int; g Int; b Int; }   // inline body, every member ends in ';'
```

### 6.2 `,` separates elements of a value collection

A `,` separates the elements of a value collection: array literals, `tuple`, `enum`, call and constructor arguments, `init{ }` fields, generic arguments, and `match` arm lists. It is **never trailing**: a `,` appears only *between* elements, never after the last one.

```zane
arr Array([Int(1), Int(2), Int(3)])
type Colors = enum [ red, green, blue ]
```

### 6.3 Newlines separate statements

A newline separates statements in a function body or a control-flow block. Zane has no statement separator, so two statements cannot share a line. This is the one place a newline is structural.

```zane
Void main() {
    x Int(5)
    print(x)
}
```

### 6.4 `{ }` versus `[ ]`

`{ }` encloses a named-typed-member body (`struct`, `class`, `variant`) and a code or `init{ }` block. `[ ]` encloses a flat list: an array, a `tuple`, an `enum`, a `match` arm list, or a function-type parameter list.

Because the parser always knows whether it is inside a type-expression body or a code block, it always knows whether a newline separates statements or is insignificant.

> **See also:** [`syntax.md`](syntax.md) §1 for declaration forms and [`adt.md`](adt.md) for how these delimiters apply across `enum`, `variant`, and `match`.

---

## 7. Design Rationale

| Decision | Rationale |
|---|---|
| Case-sensitive parsing | Casing can carry meaning only if it is never normalised. Two cases of one spelling being one identifier would erase the kind distinction below. |
| Casing determines kind | Encoding type-vs-value in the first letter removes a class of ambiguity at the lexical layer, so later grammar rules do not need declarations or lookahead to know what a bare name is. |
| Lowercase types are an error | A hard error, rather than a warning or a convention, is what lets the parser and reader rely on casing as ground truth everywhere. |
| Digits allowed inside names | Names such as `Vec2` and `Matrix3` read naturally, and with parameters supplied only through `<>` there is no need to reserve digits for a baked-in size form. |
| Casing disambiguates `<>` | Reusing `<` and `>` for type arguments is only safe because the left operand's casing distinguishes a type application from a comparison. |
| Delimiter follows the separated thing | Tying `;`, `,`, and the newline each to one kind of separated thing means a character never carries two meanings in one context, so the parser never needs lookahead to know what a separator separates. |
| `;` always trailing, `,` never trailing | A terminator that follows every member (and a separator that never follows the last element) makes both rules positional and uniform: every member ends in `;`, no value collection ends in `,`, inline or multiline. The asymmetry also keeps the two characters visually distinct in role. |
| `;` always, newlines insignificant in member bodies | Making the member delimiter explicit and uniform inline and multiline removes the need for newline-sensitivity rules inside `struct`/`class`/`variant` bodies. |
| `{ }` for bodies, `[ ]` for flat lists | One bracket marks a named-member body or code block and the other a flat list, so the bracket itself signals whether newlines are structural. |

---

## 8. Summary

| Concept | Rule |
|---|---|
| Case sensitivity | Identifiers compare by exact bytes; case is never folded |
| Type names | Uppercase-initial; a lowercase name in a type position is a compile-time error |
| Value names | Lowercase-initial; bindings, parameters, and fields |
| Package names | camelCase (lowercase-initial); appear only after `package`, after `import`, or as the left operand of `$` |
| Number parameter | A lowercase name (`n`) declared `n Number` in a `<>` header; a number, not a type |
| Type parameter | An uppercase name (`T`) declared `T Type` in a `<>` header; referenced bare |
| Digits | Legal in a name except as the first character; carry no special meaning |
| `<>` disambiguation | A type (uppercase) on the left means a type argument list; a value (lowercase) means comparison |
| Member terminator | `;` terminates every member of a `struct`/`class`/`variant` body; always trailing, inline or multiline; newlines are insignificant there |
| Value separator | `,` separates elements of a value collection (arrays, `tuple`, `enum`, call/constructor args, `init{}` fields, generic args, `match` arms); never trailing |
| Statement delimiter | A newline separates statements; there is no statement separator, so two statements cannot share a line |
| Brackets | `{ }` holds a member body or code/`init{}` block; `[ ]` holds a flat list |
