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
- **`The bracket picks the separator`.** A `{ }` body terminates each entry with `;` (always trailing); a `[ ]`, `( )`, or `< >` list separates its entries with `,` (never trailing). A `{ }` holding statements is a code block, where a newline separates.

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

Package names share lowercase-initial casing with value names but are syntactically distinct: they appear only after the `package` keyword, after `import`, after the `as` of a **whole-package** import alias, or as the left operand of `$`. The surrounding syntax, not casing, disambiguates them from ordinary value names.

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

A type parameter such as `T` is uppercase because it names a type; a number parameter such as `n` is lowercase because it names a compile-time number. A parameter is introduced by a type's `<>` header or inline in a verb's signature, and referenced by its bare name. See [`generics.md`](generics.md) §3.

> **Story:** [`stories/lexical.md`](../stories/lexical.md#casing-carries-the-kind) — "Casing carries the kind".

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

A leading `_` marks an identifier as private without changing the lexical class set by the first letter.

- A field beginning with `_` is private to methods whose first parameter is `this` for that type. See [`types.md`](types.md) §2.3.
- A named package-scope declaration beginning with `_` is private to its package. This applies to every named declaration, including types; operators are symbol-named and remain public. See [`packages.md`](packages.md) §4.

> **Story:** [`stories/lexical.md`](../stories/lexical.md#privacy-lives-in-the-name) — "Privacy lives in the name".

### 4.3 Reserved sigils

Certain leading characters are reserved and are not ordinary identifier starts:

| Sigil | Meaning | Canonical home |
|---|---|---|
| `&` | Guest type (`&Node`) | [`memory.md`](memory.md) §2 |
| `@` | Reserved compiler namespace (`@primitives$`, `@concepts$`, `@controlflow$`) | [`syntax.md`](syntax.md) §2.7 |
| `$` | Package-member separator (`packageName$member`) | [`packages.md`](packages.md) §1 |
| `'` | Loose form of a binary operator (`'*`, `'+`) | [`operators.md`](operators.md) §3.1 |

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
> **Story:** [`stories/lexical.md`](../stories/lexical.md#casing-carries-the-kind) — "Casing carries the kind".

---

## 6. Delimiters and Brackets

The **bracket picks the separator**. A `{ }` body terminates each entry with `;`; a `[ ]`, `( )`, or `< >` list separates its entries with `,`. The rule turns on the bracket alone rather than on what the construct means, so the same character never means two things in one context.

### 6.1 `;` terminates an entry inside `{ }`

A `;` **terminates** every entry of a `{ }` body: the members of a `struct` or `variant` type-definition body, marked or unmarked with `#`; the arms of a `match` block; the fields of an `init{ }`; and the entries of a field-constructor header or call site (§6.4). It is **always trailing**: every entry ends with a `;`, inline or multiline, single-entry or many, because newlines are **insignificant inside these bodies**. The last entry carries a `;` exactly like every other, so the form is uniform.

```zane
type Node = #struct {
    _id Int;
    scale Float;
    label String;
}

type Color = struct { r Int; g Int; b Int; }   // inline body, every entry ends in ';'

Vec2(x Float, y Float) => init{x; y;}
```

A `,` may still appear *inside* an entry, where it separates a nested list under §6.2 — a `match` arm's per-scrutinee selectors, or the arguments of a call in a field's initializer. The `;` terminates the entry; a `,` separates parts within one.

### 6.2 `,` separates an entry inside `[ ]`, `( )`, and `< >`

A `,` separates the entries of a `[ ]`, `( )`, or `< >` list: array literals, an `enum` body, a `match` case group, a function-type parameter list, call and constructor arguments, parameter lists, and generic arguments and headers. It is **never trailing**: a `,` appears only *between* entries, never after the last one. A list written with no bracket at all — a `match`'s scrutinees ([`syntax.md`](syntax.md) §4.8) — separates with `,` on the same terms.

```zane
arr Array([Int(1), Int(2), Int(3)])
type Colors = enum [ red, green, blue ]
Node(id Int, scale Float, label String)
```

### 6.3 Newlines separate statements

A newline separates statements in a function body or a control-flow block. Zane has no statement separator, so two statements cannot share a line. This is the one place a newline is structural.

```zane
Unit main() {
    x Int(5)
    print(x)
    return Unit()
}
```

### 6.4 Brackets and their separators

Each bracket takes exactly one separator, so the bracket predicts both the mark and its trailing behaviour:

| Bracket | Encloses | Separator |
|---|---|---|
| `{ }` | a body of entries: `struct`, `variant`, and their `#` forms; a `match` block of arms; an `init{ }`; a field-constructor header or call site | `;`, always trailing |
| `{ }` | a code block: a function body, a control-flow block, or a block argument | a newline (§6.3) |
| `[ ]` | a flat list: an array, an `enum` body, a `match` case group, a function-type parameter list | `,`, never trailing |
| `( )` | a parameter list or an argument list | `,`, never trailing |
| `< >` | a generic header or a generic argument list | `,`, never trailing |

A `{ }` is the one bracket with two readings, and the two are told apart by what the entries are rather than by lookahead: a body holds `;`-terminated entries, a code block holds statements. Every `{ }` is introduced by a token that says which it is — a mould keyword, `match`, `init`, a type name, or a verb's signature — so the parser always knows both which separator applies and whether a newline is structural.

> **See also:** [`syntax.md`](syntax.md) §1 for declaration forms and [`adt.md`](adt.md) for how these delimiters apply across `enum`, `variant`, and `match`.
> **Story:** [`stories/lexical.md`](../stories/lexical.md#the-bracket-picks-the-separator) — "The bracket picks the separator".
> **Story:** [`stories/lexical.md`](../stories/lexical.md#a-delimiter-for-each-separated-thing) — "A delimiter for each separated thing" tells where the two marks and the trailing asymmetry came from.

---

## 7. Summary

| Concept | Rule |
|---|---|
| Case sensitivity | Identifiers compare by exact bytes; case is never folded |
| Type names | Uppercase-initial; a lowercase name in a type position is a compile-time error |
| Value names | Lowercase-initial; bindings, parameters, and fields |
| Package names | camelCase (lowercase-initial); appear only after `package`, after `import`, after the `as` of a whole-package import alias, or as the left operand of `$` |
| Number parameter | A lowercase name (`n`) declared `n Number` (in a type's `<>` header or inline in a verb); a number, not a type |
| Type parameter | An uppercase name (`T`) declared `T Type` (in a type's `<>` header or inline in a verb); referenced bare |
| Digits | Legal in a name except as the first character; carry no special meaning |
| Leading `_` | A field is private to `this` methods for its type; a named package-scope declaration is private to its package |
| Leading `&` | `&Node` is a guest type, legal in storage, parameter, and return positions; it is the only marker a type may carry, and it is never written on `this` |
| `<>` disambiguation | A type (uppercase) on the left means a type argument list; a value (lowercase) means comparison |
| Entry terminator | `;` terminates every entry of a `{ }` body (`struct`/`variant` members marked or unmarked with `#`, `match` arms, `init{ }` fields, field-constructor entries); always trailing, inline or multiline; newlines are insignificant there |
| Entry separator | `,` separates the entries of a `[ ]`, `( )`, or `< >` list (arrays, `enum`, `match` case groups, function-type parameter lists, call/constructor args, parameter lists, generic args and headers); never trailing |
| Statement delimiter | A newline separates statements; there is no statement separator, so two statements cannot share a line |
| Brackets | The bracket picks the separator: `{ }` takes `;` (or newlines, as a code block), `[ ]`/`( )`/`< >` take `,` |
