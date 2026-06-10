# Zane Type Generics and Type Parameters

This document specifies Zane's two parameter kinds that attach to type declarations: **type generics** (formerly written in `<'T>` slots) and **type parameters** (formerly written in `[name]` slots). Zane's type-generic system is fully inferred: the language provides no syntax for declaring type generics at the declaration header or for supplying them at call sites. Type parameters remain explicit at definition and use sites.

> **See also:** [`syntax.md`](syntax.md) §2 for the surface syntax of type expressions. [`oop.md`](oop.md) §3.9.2 for the generic-match phase of overload resolution, which is where inference happens at call sites.

---

## 1. Overview

Zane supports two parameter kinds, both attached to type declarations.

- **`Two parameter kinds`.** Zane has two parameter kinds attached to types: *type generics* (which range over types) and *type parameters* (which range over compile-time integer values). The names align with the syntax: type parameters take a value, generics take a type.
- **`Inferred type generics`.** The compiler infers type generics from `'`-prefixed names that appear in type positions inside a declaration body. The language provides no syntax for declaring type generics at the declaration header or for supplying them at call sites.
- **`Type parameters stay explicit`.** Type parameters are baked into the declaration header as `[name]` segments and supplied at use sites as integer literals embedded in the type name (for example, `Array10`). They cannot be inferred because their values are not available at runtime.
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

Type parameters are declared with square brackets in the type header and range over compile-time integers. They are unaffected by the inferred-type-generic rule.

```zane
struct Matrix[rows]X[cols] {
    ...
}
```

`Matrix[10]X[20]` is a valid use site: the type parameters take integer literals. The type generics of the body (if any) are inferred at the call site.

> *Example.* `Array10` is a `Array` with type parameter `10`. The element type of the array is a type generic, inferred from the surrounding context at each use site.

### 2.3 Kinds are distinct

Type generics and type parameters live in different kinds. A type parameter **MUST NOT** be supplied where a type generic is expected and vice versa. The two never substitute for each other syntactically or semantically.

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
    data Array[10]
}
```

`Wrap` has one type generic (the element type of `data`). The body has no `'`-prefixed name to anchor the type variable, so the type generic is unnamed in the source. The call site fixes it by passing an argument whose type drives the inference, or by a type ascription on the call.

> *Rationale.* Unifying an `Array` element type with a named `'T` in a sibling field lets the same type variable appear in many positions without writing a binder. This is the inferred-generics equivalent of "the element type of data" without naming it explicitly.

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
Array[cols] rowAt(this Matrix[rows]X[cols], i Int) {
    ...
}
```

This method's named type-generic set is empty (no `'`-prefixed name appears in any type position), but its return type's element type is an unnamed type generic that the call site must fix. The type parameters `rows` and `cols` come from the receiver type and are not type generics.

### 4.2 Type generics reachable only through the return type

A type generic that appears only in the return type is allowed, but every call site **MUST** supply a type ascription that fixes it. The most common form is a type-annotated declaration:

```zane
result Int = defaultInt()
```

The type ascription `Int` fixes the type generic to `Int`. A bare call without a type ascription that fixes every type generic is a compile-time error: *cannot infer type generic*.

### 4.3 Method-site `this` binders

`this` types may bind type parameters; this is unchanged by the inferred-type-generic rule. A method that binds type parameters in its `this` type is implicitly parameterized over those type parameters and is not generic over any type unless one appears in a type position elsewhere in its signature.

---

## 5. Use-Site Inference

### 5.1 No type-argument syntax exists

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

At a call site, the compiler unifies each parameter type with the static type of the corresponding argument. Distinct occurrences of the same `'`-prefixed name inside the called declaration are unified to the same type variable; distinct `'`-prefixed names are independent type variables.

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

### 6.1 Literal-only at use site

Type-parameter arguments at use site **MUST** be integer literals. Runtime values are illegal.

```zane
Matrix10X20   // ok
Matrix[n]X[m] // ILLEGAL
```

### 6.2 No arithmetic in type arguments

Arithmetic on type parameters in type arguments is **not specified**. Expressions such as `Array[rows*cols]` are currently illegal.

This is deferred because the language does not yet define how type-level arithmetic expressions should be compared for equality. Without that rule, expressions such as `N+M` and `M+N` would have ambiguous equivalence status.

### 6.3 Type generics are unaffected by the literal-only rule

The literal-only rule applies only to type parameters. Type generics have no use-site form, so the rule is vacuous for them. The two kinds remain distinct because a type-parameter binder (`[rows]X[cols]`) and a type-generic name (`'T`) appear in different syntactic positions and identify different things.

---

## 7. The `Array[size]` Storage Primitive

### 7.1 Compiler-provided layout

`Array[size]` is a compiler-provided storage primitive representing `size` contiguous elements of an inferred type. Its byte size is `size * sizeof(elementType)`. It has no header. The element type is inferred from the surrounding context, just like any other type generic in the language.

### 7.2 Array is the fixed-size storage base case

Other fixed-size container types (e.g., vectors, matrices) are defined in terms of `Array` and do not require compiler support. Dynamic container types are not specified in this document; when they are specified, they are separate runtime-managed wrapper types over opaque `@primitives$...` storage primitives rather than extensions of `Array`.

---

## 8. Deferred Features

The following are intentionally not specified in this version of the spec:

- arithmetic on type parameters in type positions
- dynamic container types such as lists and maps
- bounds-checking rules for element access APIs
- named lane access (`.x`, `.y`, `.z`, `.w`)
- phantom type generics (see §3.5)
- use-site type ascriptions for type-generic specification beyond argument-driven inference

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| Two parameter kinds: type generics and type parameters | A type parameter takes a value (compile-time integer); a type generic takes a type. Naming each kind after the *kind of thing* it accepts (value vs. type) keeps the terminology aligned with the syntax. |
| Type generics are inferred from `'`-prefixed names in type positions | Removes the binder syntax entirely and makes the type-generic set of a declaration discoverable by walking the body, mirroring ML-style polymorphism. |
| No `<'T>` at the definition site | The struct/function/method header has no list of type generics to maintain. The body declares them implicitly. |
| No `<...>` at the use site | Callers never write type arguments; the compiler unifies argument types with the called declaration's parameters. This removes a major source of deep nested type expressions at call sites. |
| Same name in two positions = same type generic | `'`-prefixed names act as type variables within a body, unifying occurrences automatically. Different names are independent type variables. |
| `Array[size]` element type is unnamed and inferred | Keeps the storage primitive consistent with the rest of the inferred-generics design. A named `'T` in a sibling field is unified with the element type by the compiler. |
| Phantom type generics are rejected | Without use-site syntax, phantom type generics cannot be specified. A future revision may add a separate type-ascription rule to support them. |
| Type parameters remain explicit at definition and use sites | Type parameters cannot be inferred from runtime values; explicit `[name]` binders and use-site literals are required. |
| Concept-typed literals require explicit destination type at call sites | The compiler cannot choose between `Int`, `Float`, and other concrete number types from a bare literal. The `Int(2)` wrap is a small explicit cost that replaces deeper `<...>` argument lists elsewhere. |

---

## 10. Summary

| Concept | Rule |
|---|---|
| Type generic | A `'`-prefixed name in a type position inside a declaration body; collected by walking the body |
| Type parameter | A `[name]` segment in the type header; supplied at use site as an integer literal in the type name |
| Definition site (type generic) | No `<'T>` list; type generics are collected from the body |
| Use site (type generic) | No `<...>` argument list; the compiler infers type generics from argument types and type ascriptions |
| Definition site (type parameter) | A `[name]` segment in the type header |
| Use site (type parameter) | An integer literal embedded in the type name (e.g., `Array10`) |
| Same name in two positions | Same type generic |
| Different names in two positions | Independent type generics |
| Unnamed type generics | Allowed when inferred from context (e.g., `Array[size]` element types) |
| Phantom type generics | Not allowed |
| Element type of `Array[size]` | Inferred from context |
| Concept-typed literal in inferred-generic position | Must be wrapped in its destination core type |
