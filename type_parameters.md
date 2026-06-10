# Zane Type Generics and Type Parameters

This document specifies Zane's two parameter kinds that attach to type declarations: **type generics** (formerly written in `<'T>` slots) and **type parameters** (formerly written in `[name]` slots). Zane's type-generic system is fully inferred: the language provides no syntax for declaring type generics at the declaration header or for supplying them at call sites. Type parameters use a binder/reference/root form — they are neither a type nor a value, but a third kind of symbol that resolves to a value (`Int`) in body positions.

> **See also:** [`syntax.md`](syntax.md) §2 for the surface syntax of type expressions. [`oop.md`](oop.md) §3.9.2 for the generic-match phase of overload resolution, which is where inference happens at call sites.

---

## 1. Overview

Zane supports two parameter kinds, both attached to type declarations.

- **`Two parameter kinds`.** Zane has two parameter kinds attached to types: *type generics* (which range over types) and *type parameters* (which are a third kind of symbol — neither a type nor a value — that resolves to an `Int` value in body positions). The names align with the syntax: type parameters take a value-like role, generics take a type.
- **`Inferred type generics`.** The compiler infers type generics from `'`-prefixed names that appear in type positions inside a declaration body. The language provides no syntax for declaring type generics at the declaration header or for supplying them at call sites.
- **`Type parameters are symbols with three forms`.** A type parameter is introduced as a binder (`[name]` in a type header), referenced as a name inside other `[name]` slots in the same scope, or rooted by baking an integer into the type identifier (`Array10`). Rooted forms are how types "start the chain" without referencing an outer binder.
- **`Array is the storage primitive`.** `Array[size]` is the single compiler-provided fixed-size storage primitive; its element type is a type generic and is inferred from context.

---

## 2. Parameter Kinds

### 2.1 Inferred type generics

A type generic is introduced by a `'`-prefixed name in a type position inside a declaration body. The compiler walks the body and collects every `'`-prefixed name; the set of unique names is the type-generic set of the declaration. There is no separate binder syntax — the `'`-prefixed name is simultaneously the introduction and the reference.

```zane
struct Box {
    value 'T
}
```

`Box` has one type generic. Calling `Box(Int(7))` instantiates it with `'T = Int`.

### 2.2 Type parameters

Type parameters are declared with square brackets in the type header and are a third kind of symbol — neither a type nor a value. They participate in three syntactic forms (see §2.4): binder, reference, and root. The same `n` symbol may be bound in one header and referenced inside other type expressions in the same scope.

```zane
struct Buffer[n] {
    data Array[n]
}
```

`Buffer` declares one type-parameter symbol `n` in its header, and `data` references that same `n` in a nested `Array` slot. The reference is legal because `n` is in scope from the enclosing declaration. The body of a method on `Buffer` may also use `n` as a value (it resolves to `Int`):

```zane
Int size(this Buffer[n]) {
    return n
}
```

Here `n` in the body is read as the `Int` value the caller supplied for that type-parameter symbol. The `Array[n]` storage layout uses the same value to determine byte size.

### 2.3 Kinds are distinct

Type generics and type parameters live in different kinds. A type parameter **MUST NOT** be supplied where a type generic is expected and vice versa. The two never substitute for each other syntactically or semantically.

### 2.4 Type-parameter symbol forms

A type-parameter symbol has three syntactic forms and one lexical rule.

| Form | Syntax | Role | Example |
|---|---|---|---|
| **Binder** | `[name]` in a type header or in a method `this` parameter type | Introduces a fresh type-parameter symbol in scope | `struct Buffer[n] { ... }` |
| **Reference** | `[name]` in any other type expression in the same scope | Refers to an in-scope type-parameter symbol | `data Array[n]` (refers to the header's `n`) |
| **Root** | integer literal baked into the type identifier | A type-parameter symbol with no source — the start of a chain | `Array3`, `Matrix10X20` |
| **Adjacency** (lexical rule) | Two type-parameter slots in the same type name | A non-type-parameter delimiter is required between adjacent slots so the lexer can determine where each slot begins and ends | `Buffer[n]X[m]` ok; `Buffer[n][m]` ILLEGAL |

The first three rows are the three *syntactic forms* of a type-parameter symbol: binder, reference, and root. The fourth row — **Adjacency** — is a *lexical rule* that governs how multiple slot forms may be combined in a single type name; it is not a fourth form of the symbol itself.

The bracket form always means "binder or reference." It is never legal to put a bare integer literal inside `[]`; an integer literal is a *value*, not a name. If `10` appears inside brackets where no symbol named `10` is in scope, the program is rejected: an integer literal is not a type-parameter symbol. The integer must instead be baked into the type name as the root form (`Array10`).

Two type-parameter slots written next to each other (for example `Buffer[n][m]`) require at least one non-type-parameter character between them. The bracket sequence is not a self-delimiting form: a delimiter character is needed to mark where one slot ends and the next begins. The convention used throughout this spec is an uppercase letter (`Matrix[rows]X[cols]`, `Buffer[n]X[m]`), but any non-type-parameter identifier character serves.

A type-parameter symbol referenced inside a body position (not inside `[]`) resolves to its `Int` value. This is how the body of a method on `Buffer[n]` can read `n` as an integer.

A type-parameter symbol or type generic introduced in any declaration in a package is in scope in every other declaration in the same package, because the package compiles as one unit.

---

## 3. Inferred Type Generics in Detail

### 3.1 Collection from the body

The compiler walks the type positions of a declaration — struct field types, function parameter types, function return types, method `this` types, method parameter types, method return types — and collects every `'`-prefixed name. Each unique name becomes a distinct type generic.

```zane
struct Vector {
    x 'T
    y 'T
}

// Vector has one type generic, "T", used by both fields.

struct KeyValue {
    key 'K
    value 'V
}

// KeyValue has two type generics, "K" and "V", independent.
```

### 3.2 Same name = same type; different names = different types

A type generic is identified by its `'`-prefixed name. Two occurrences of `'T` inside the same body are the same type generic; two different names `'T` and `'U` are independent type generics that may be instantiated to different types.

```zane
struct Pair {
    first 'T
    second 'U
}

// Pair is polymorphic over T and U; first and second are independent.
```

### 3.3 The `'` prefix is part of the identifier

A `'`-prefixed name in a type position is a type generic, not a reference to a concrete type. The names `'T` and `T` are different identifiers, and a concrete type named `T` does not collide with a type generic named `'T`. This rule keeps type-generic references unambiguous inside a body.

### 3.4 Element types of `Array[size]` are unnamed type generics

The element type of an `Array[size]` is never written; it is always an unnamed type generic that the compiler tracks internally. If the surrounding body contains a `'`-prefixed name in a type position that the compiler can unify with the element type, the element type takes that name. Otherwise the element type is a fresh type variable that the call site must fix.

```zane
struct Wrap {
    data Array
}
```

`Wrap` has one type generic (the element type of `data`). The body has no `'`-prefixed name to anchor the type variable, so the type generic is unnamed in the source. The call site fixes it by passing an argument whose type drives the inference, or by a type ascription on the call.

> *Rationale.* Unifying an `Array` element type with a named `'T` in a sibling field lets the same type variable appear in many positions without writing a binder. This is the inferred-generics equivalent of "the element type of data" without naming it explicitly.

> *Note.* Writing `Array[10]` here is **not** the way to anchor a type generic — the `10` would be interpreted as a type-parameter-symbol reference, and an integer literal is not a name. See §6.1.

### 3.5 Phantom type generics are not allowed

Every type generic in a declaration **MUST** be determinable from the static type of a call argument or a type ascription on the call. A type generic that has no path from any parameter type, return type, or receiver type to a call argument is a *phantom* type generic and is a compile-time error.

```zane
struct Tag {
    _marker 'Brand
}

// ILLEGAL: 'Brand has no source from any argument. There is no way to
// determine which 'Brand a call site is asking for.
```

Phantom type generics are rejected because the inferred-generics design offers no use-site syntax to specify them. A future spec revision may add support for phantom type generics via a separate type-ascription rule; that rule is not part of this revision.

> **See also:** [`oop.md`](oop.md) §3.9.2 for the generic-match phase of overload resolution, which is where the compiler unifies call-argument types with declared parameter types.

---

## 4. Method Signatures and `this` Binders

### 4.1 Methods are also inferred-generic

A method is generic in the same way as a struct or function: the compiler walks the method's `this` type, parameter types, and return type, and collects `'`-prefixed names from each.

```zane
Array[n] rowAt(this Buffer[n], i Int) {
    ...
}
```

This method's named type-generic set is empty, but its return type's element type is an unnamed type generic that the call site must fix. The type-parameter symbol `n` comes from the receiver type, is in scope, and may be read as an `Int` inside the body.

### 4.2 Type generics reachable only through the return type

A type generic that appears only in the return type is allowed, but every call site **MUST** supply a type ascription that fixes it. The most common form is a type-annotated declaration:

```zane
result Int = defaultInt()
```

The type ascription `Int` fixes the type generic to `Int`. A bare call without a type ascription that fixes every type generic is a compile-time error: *cannot infer type generic*.

### 4.3 Method-site `this` binders

`this` types may bind type-parameter symbols; this is unchanged by the inferred-type-generic rule. A method that binds type-parameter symbols in its `this` type is implicitly parameterized over those symbols, and the symbols may be referenced or read as `Int` values inside the method body.

---

## 5. Use-Site Inference

### 5.1 No type-argument syntax exists for type generics

The language provides no surface form for type arguments. A caller **MUST NOT** write angle brackets or any other syntax to supply type generics. The compiler always infers them.

```zane
// ILLEGAL: there is no <'T> form anywhere in the language
box Box<'Int>(Int(7))
```

The legal form is:

```zane
box Box = Box(Int(7))   // 'T inferred as Int
```

### 5.2 Inference is argument-driven with type-ascription fallback

At a call site, the compiler unifies each parameter type with the static type of the corresponding argument. Type generics are inferred from the *type* part of the argument; type-parameter symbols are inferred from the *type-parameter* part. Distinct occurrences of the same `'`-prefixed name inside the called declaration are unified to the same type variable; distinct `'`-prefixed names are independent type variables.

If a type generic is reachable only through the return type, the call site **MUST** supply a type ascription that fixes it. The most common form is a type-annotated declaration:

```zane
result Int = defaultInt()
```

When a call site lacks both an argument and a type ascription for some type generic, the call is a compile-time error: *cannot infer type generic*.

### 5.3 Concept types and literal arguments

Source literals carry compiler concept types such as `@concepts$Number` or `@concepts$Text`. A bare literal at a call site does not carry a concrete storage type and **MUST NOT** be used to instantiate a type generic directly, because the compiler has no way to choose between `Int`, `Float`, and other concrete number types.

Call sites that pass literals to inferred-generic parameters **MUST** wrap the literal in the destination core type:

```zane
vec Vector = Vector(Int(2), Int(15))   // legal: each argument is a concrete Int

vec Vector = Vector(2, 15)             // ILLEGAL: literals cannot drive inference of 'T
```

This is the only ergonomic cost of the inferred-generics design, and it is a deliberate trade-off: a single explicit wrap at the call site replaces a potentially-deep `<...>` type-argument list at every use site.

### 5.4 Inference failure and error messages

When the compiler cannot infer a type generic from a call, it reports a compile-time error that names the uninferable type generic(s) and points at the relevant call site. The error **SHOULD** also surface the binding declaration's name and the parameter position that would have driven inference if the argument had carried a concrete type.

Definition-site phantom-type-generic errors (see §3.5) are caught at the declaration, not the call site, because there is no parameter to source the type from.

---

## 6. Type-Parameter Restrictions

### 6.1 Integer literals are not type-parameter symbols

A bracket form (`[name]`) is a binder or a reference. The thing inside the brackets **MUST** be a name. An integer literal (such as `10`) is a value, not a name, and **MUST NOT** appear inside brackets.

```zane
struct Buffer[n] {
    data Array[10]   // ILLEGAL: 10 is a literal, not a type-parameter symbol in scope
}
```

To start a chain with a concrete size, drop the brackets and bake the integer into the type name (the *root* form from §2.4):

```zane
colors Array3 = Array3([145, 134, 47])
```

`Array3` is a complete type with a baked-in type-parameter value of 3; no outer reference is needed to make sense of it.

This rule applies in particular to the `data Array[10]` shape seen in §3.4.

### 6.2 Literal-only at use site (root form)

A type-parameter symbol supplied as a root form **MUST** be an integer literal. Runtime values are illegal in the root form.

```zane
Array10    // ok
Array[n]   // ILLEGAL outside a binder/reference context — see §2.4
```

### 6.3 No arithmetic in type arguments

Arithmetic on type-parameter symbols in type arguments is **not specified**. Expressions such as `Array[rows*cols]` are currently illegal.

This is deferred because the language does not yet define how type-level arithmetic expressions should be compared for equality. Without that rule, expressions such as `N+M` and `M+N` would have ambiguous equivalence status.

### 6.4 Type generics are unaffected by the type-parameter rules

The integer-literal and arithmetic rules apply only to type-parameter symbols. Type generics have no use-site form, so these rules are vacuous for them. The two kinds remain distinct because a type-parameter binder (`[rows]X[cols]`) and a type-generic name (`'T`) appear in different syntactic positions and identify different things.

---

## 7. The `Array[size]` Storage Primitive

### 7.1 Compiler-provided layout

`Array[size]` is a compiler-provided storage primitive representing `size` contiguous elements of an inferred type. Its byte size is `size * sizeof(elementType)`. It has no header. The element type is inferred from the surrounding context, just like any other type generic in the language. The `size` itself is a type-parameter symbol — either an in-scope reference (e.g., the header's `n` in `data Array[n]`), a baked-in root form (`Array3`), or absent in a body field position (in which case the size is an unnamed type-parameter value that the call site fixes together with the element type; see §3.4).

### 7.2 Array is the fixed-size storage base case

Other fixed-size container types (e.g., vectors, matrices) are defined in terms of `Array` and do not require compiler support. Dynamic container types are not specified in this document; when they are specified, they are separate runtime-managed wrapper types over opaque `@primitives$...` storage primitives rather than extensions of `Array`.

---

## 8. Deferred Features

The following are intentionally not specified in this version of the spec:

- arithmetic on type-parameter symbols in type positions
- dynamic container types such as lists and maps
- bounds-checking rules for element access APIs
- named lane access (`.x`, `.y`, `.z`, `.w`)
- phantom type generics (see §3.5)
- use-site type ascriptions for type-generic specification beyond argument-driven inference

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| Two parameter kinds: type generics and type parameters | A type generic takes a type; a type parameter is a third kind of symbol that resolves to a value (`Int`) in body positions. Naming each kind after the *kind of thing* it accepts (type vs. value) keeps the terminology aligned with the syntax. |
| Type generics are inferred from `'`-prefixed names in type positions | Removes the binder syntax entirely and makes the type-generic set of a declaration discoverable by walking the body, mirroring ML-style polymorphism. |
| No `<'T>` at the definition site | The struct/function/method header has no list of type generics to maintain. The body declares them implicitly. |
| No `<...>` at the use site | Callers never write type arguments; the compiler unifies argument types with the called declaration's parameters. This removes a major source of deep nested type expressions at call sites. |
| Same name in two positions = same type generic | `'`-prefixed names act as type variables within a body, unifying occurrences automatically. Different names are independent type variables. |
| `Array[size]` element type is unnamed and inferred | Keeps the storage primitive consistent with the rest of the inferred-generics design. A named `'T` in a sibling field is unified with the element type by the compiler. |
| Phantom type generics are rejected | Without use-site syntax, phantom type generics cannot be specified. A future revision may add a separate type-ascription rule to support them. |
| Type parameters are a third kind of symbol with three forms | They are not types (so they don't go in type-generic slots) and not values (so they can't be runtime arguments). The binder/reference/root form is the smallest syntax that supports in-scope references, header binders, and concrete root types. |
| Brackets `[name]` are binder-or-reference; literals are not names | An integer literal is a value, so it cannot be a name. The root form (`Array10`) is the only way to write a type-parameter symbol without an in-scope reference. |
| `Array[10]` in a struct body is illegal | The `10` is a literal with no in-scope binder named `10`. The body must reference a header-binder (`Array[n]`) or accept an unnamed type-generic element. |
| Adjacent type-parameter slots require a delimiter | A bracket sequence is not self-delimiting, so the lexer cannot tell where one slot ends and the next begins without a non-type-parameter character between them. |
| Concept-typed literals require explicit destination type at call sites | The compiler cannot choose between `Int`, `Float`, and other concrete number types from a bare literal. The `Int(2)` wrap is a small explicit cost that replaces deeper `<...>` argument lists elsewhere. |

---

## 10. Summary

| Concept | Rule |
|---|---|
| Type generic | A `'`-prefixed name in a type position inside a declaration body; collected by walking the body |
| Type parameter | A third kind of symbol — neither a type nor a value — with three syntactic forms (binder, reference, root) |
| Type-parameter binder | `[name]` in a type header; introduces a fresh type-parameter symbol in scope |
| Type-parameter reference | `[name]` in a nested type expression in the same scope; refers to an in-scope type-parameter symbol |
| Type-parameter root | An integer literal baked into the type identifier (`Array10`); a type-parameter symbol with no source |
| Adjacent type-parameter slots | A non-type-parameter delimiter is required between them so the lexer can split the name |
| Inside `[]` | Must be a name (binder or reference); an integer literal is rejected |
| Type-parameter symbol in body | Resolves to the `Int` value the caller supplied |
| Definition site (type generic) | No `<'T>` list; type generics are collected from the body |
| Use site (type generic) | No `<...>` argument list; the compiler infers type generics from argument types and type ascriptions |
| Same name in two positions | Same type generic |
| Different names in two positions | Independent type generics |
| Unnamed type generics | Allowed when inferred from context (e.g., `Array[size]` element types) |
| Phantom type generics | Not allowed |
| Element type of `Array[size]` | Inferred from context (always a type generic) |
| Concept-typed literal in inferred-generic position | Must be wrapped in its destination core type |
