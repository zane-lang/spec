# Zane Generics and Type Parameters

This document specifies Zane's unified type-parameter system. A type in Zane is a *templated function*: it takes parameters and is executed to produce a concrete layout. There is now a single kind of parameter — supplied positionally in `<>` slots — that ranges over either types (`'T`) or numbers (`n`). Type arguments are written in `<>` type expressions; they are never written at a constructor call.

> **See also:** [`syntax.md`](syntax.md) §2 for the surface syntax of type expressions. [`types.md`](types.md) §5 for `type` and `alias` declarations and §3 for constructors. [`lexical.md`](lexical.md) §3 for the casing rule that distinguishes types from values. [`functions.md`](functions.md) §5 for the generic-match phase of overload resolution.

---

## 1. Overview

Zane treats a type as something that is *executed*. A type definition takes parameters and produces a concrete layout, the same way a function takes parameters and produces a value. Templating is therefore not a bolt-on feature; it falls directly out of types being executable.

- **`Types are templated functions`.** A type definition declares parameters in a `<>` header and produces a layout. Applying arguments to those parameters (`Vector<Int>`) evaluates the template into a concrete type.
- **`One unified parameter system`.** A type parameter is either a *type parameter* (written `'T`) or a *number parameter* (written as a lowercase name such as `n`). Both are supplied positionally in the same `<>` slots; only the marker distinguishes them.
- **`<>` describes architecture, `()` constructs values`.** A `<>` type expression is a compile-time description that lives in the type system. A `()` constructor call is a runtime construction that lives in the value system. They are different mechanisms, not two syntaxes for one idea.
- **`No type arguments at constructor calls`.** A constructor is always called by its bare name. A constructor never takes a `<>` list. When a constructor needs a type, the type is passed *as an argument* (`Vector(Int)`) or inferred from the value arguments.
- **`Array is the storage primitive`.** `Array<'T, n>` is the single compiler-provided fixed-size storage primitive: `n` contiguous elements of type `'T`.

---

## 2. Types as Templated Functions

### 2.1 A type definition takes parameters

A parameterized type declares its parameters in a `<>` header, then produces a layout. The header is the type's *signature*; the right-hand side is what it produces.

```zane
type Vector<'T> = struct {
    x 'T
    y 'T
}
```

`Vector` is a template with one type parameter, `'T`. It is not yet a concrete type. Applying an argument evaluates it:

```zane
Vector<Int>     // evaluates the template with 'T = Int
Vector<Float>   // evaluates the template with 'T = Float
```

Each distinct application is a distinct concrete type with a known, fixed layout.

### 2.2 Parameters may be types or numbers

A header may mix type parameters and number parameters. A number parameter is an ordinary lowercase name; the compiler knows it is a number because of its casing (see [`lexical.md`](lexical.md) §3).

```zane
type Buffer<'T, n> = struct {
    data Array<'T, n>
}

Buffer<Int, 64>   // 'T = Int, n = 64
```

`'T` ranges over types; `n` ranges over compile-time numbers. Inside the body, `n` may also be read as a number value — for example, to compute a length — exactly as `data Array<'T, n>` uses it to fix the storage size.

### 2.3 Concrete (non-parameterized) types

A type with no parameters needs no `<>` header. Its `<>`-free name is already a complete type.

```zane
type Wrapper = struct {
    vec Vector<Int>
    arr Array<Int, 10000>
}
```

`Wrapper` is concrete: every field type is fully applied, so `Wrapper`'s layout is known. `struct Name { ... }` and `class Name { ... }` (see [`types.md`](types.md) §2) are the field-declaring shorthand for `type Name = struct { ... }` and `type Name = class { ... }`.

---

## 3. The Unified Parameter System

### 3.1 Two parameter kinds, one slot system

A parameter is identified by its marker, not by a separate syntactic position:

| Kind | Written | Ranges over | Example |
|---|---|---|---|
| **Type parameter** | `'`-prefixed uppercase name (`'T`) | Types | `Vector<'T>` |
| **Number parameter** | lowercase name (`n`) | Compile-time numbers | `Buffer<'T, n>` |

Both kinds occupy the same positional `<>` slots and the same constructor parameter list. This is the unification: there is no longer a separate bracket form for sizes and a separate apostrophe form for types living in different positions. The two kinds differ only in what they range over.

### 3.2 The `'` prefix is part of the identifier

A `'`-prefixed name is a type parameter, not a reference to a concrete type. `'T` and `T` are different identifiers, and a concrete type named `T` does not collide with a type parameter named `'T`. This keeps type-parameter references unambiguous inside a body.

### 3.3 Same name = same parameter; different names = independent

Within one declaration, two occurrences of `'T` are the same type parameter, and two different names (`'T` and `'U`) are independent. The same rule applies to number parameters.

```zane
type Pair<'T, 'U> = struct {
    first 'T
    second 'U
}
```

`first` and `second` are independent; a use site may apply different types to each (`Pair<Int, Float>`).

### 3.4 Number parameters resolve to values in body positions

A number parameter referenced in a body position (not a type position) resolves to its number value. This is how a method on a parameterized type can read a size as an ordinary number.

```zane
Int size(this Buffer<'T, n>) {
    return n
}
```

Here `n` in the return position is the number the use site supplied for that parameter. The `Array<'T, n>` layout uses the same value to fix the storage size.

> **See also:** [`effects.md`](effects.md) §2 — a number parameter read in a body position is a read-only value-like binding.

---

## 4. Type Expressions (`<>`)

### 4.1 `<>` is the application syntax of the type system

A type expression applies arguments to a parameterized type. It is a compile-time, structural description — it says what a value's architecture *is*, it does not build a value. Type expressions appear wherever a type is expected:

```zane
type Wrapper = struct {
    vec Vector<Int>
    arr Array<Int, 10000>
}

Vec2 transform(this Vec2, mat Matrix<Float, 3>) { ... }
```

Struct fields, function and method signatures, return types, aliases, and nested arguments are all type-expression positions.

### 4.2 Arguments are positional

Arguments fill the header's parameters left to right. A type argument fills a `'T` slot; a number argument fills a number slot.

```zane
Array<Int, 10000>     // 'T = Int, n = 10000
Matrix<Float, 3>      // type then number
```

A type argument may itself still be parametric when the surrounding scope has the parameter in scope. Inside a declaration that introduces `'T` and `n`, the expression `Array<'T, n>` forwards both:

```zane
type Buffer<'T, n> = struct {
    data Array<'T, n>
}
```

### 4.3 Casing makes `<>` unambiguous

The token before `<` in a type expression is always a type, which is always uppercase-initial (see [`lexical.md`](lexical.md) §3). A comparison operator never has a type on its left. This is why `Vector<Int>` parses as a type application while `a < b` parses as a comparison: the casing of the left operand decides. Case-sensitive parsing is what makes the `<>` type syntax viable.

---

## 5. Constructor Calls (`()`)

### 5.1 `()` is the construction syntax of the value system

A constructor call builds a value at run time. It is a different mechanism from a type expression: `Vector<Int>` describes a type, while `Vector(...)` builds a `Vector` value. A constructor is always invoked by its bare name.

### 5.2 A constructor never takes a `<>` list

Type arguments belong to type expressions only. A constructor call **MUST NOT** be written with `<>`.

```zane
vec Vector(Int(2), Int(3))   // legal: bare-name constructor call
vec Vector<Int>(Int(2))      // ILLEGAL: a constructor call takes no <> list
```

When a constructor needs to know a type parameter, that information reaches it in one of three ways.

### 5.3 Type inferred from value arguments

When every constructor argument is a value, the type is inferred from what is passed. The type is never written — only deduced.

```zane
vec Vector(Int(2), Int(3))   // 'T inferred as Int from the arguments
```

### 5.4 Type passed as an argument

A type can be passed directly as a constructor argument. This is not inference: the type itself is the argument. The constructor declares this with a `type` parameter kind.

```zane
vec Vector(Int)              // Int passed as the type argument
```

```zane
Vector(T type) {
    return init{
        x: T(0),
        y: T(0)
    }
}
```

A `T type` parameter accepts a type as a value-level argument and makes it usable as a type inside the body (for example, `T(0)`).

### 5.5 Number passed as an argument

A number parameter is passed the same way, declared with the `number` parameter kind. `number` is a keyword denoting the number kind.

```zane
arr Array(Int, 10000)        // Int passed as the type, 10000 passed as the size

Array(T type, n number) {
    // zero-initialise n elements of type T
}
```

### 5.6 Concept-typed literals must be wrapped

A bare source literal carries a compiler concept type (such as `@concepts$Number`), not a concrete storage type. A bare literal **MUST NOT** drive inference of a type parameter, because the compiler cannot choose between `Int`, `Float`, and other concrete types. Wrap the literal in its destination type:

```zane
vec Vector(Int(2), Int(3))   // legal: each argument is a concrete Int
vec Vector(2, 3)             // ILLEGAL: literals cannot drive inference of 'T
```

This single explicit wrap at the call site is the deliberate cost that replaces a `<>` type-argument list at every constructor call.

---

## 6. Array Construction

`Array<'T, n>` illustrates all three mechanisms working together. The element type and the size are both parameters; a constructor supplies them by inference or by argument.

### 6.1 Inferred from a literal

```zane
arr Array([1, 2, 3])         // 'T = Int and n = 3 inferred from the literal
```

The constructor reads the literal's elements and length and fixes both parameters.

### 6.2 Explicit type and size

```zane
arr Array(Int, 10000)        // 'T = Int passed, n = 10000 passed
```

Used when no literal is available — for example, a zero-initialised array of known length.

### 6.3 Constructor overloads

```zane
Array(T type, n number) {        // called as Array(Int, 10000)
    // zero-initialise n elements of type T
}

Array(values Array<'T, n>) {     // called as Array([1, 2, 3])
    // 'T and n inferred from the literal; n is a number because it is lowercase
}
```

### 6.4 Type expression

```zane
type Matrix = struct {
    data Array<Float, 9>     // architecture: 9 contiguous Floats, stored inline
}
```

---

## 7. Why Size Must Be in the Type

### 7.1 The cost is loss of uniform stride

It is tempting to leave array size out of the type, since the stack pointer is just a register and an array never resizes after construction. C99 VLAs do exactly this for locals. The real cost of a runtime-sized type is not stack allocation — it is the **loss of uniform stride**. If two values of one type can have different sizes, the following all break:

- `arr[i]` — indexing needs `base + i * stride`, and `stride` must be a compile-time constant for O(1) access.
- Embedding in a struct — the outer struct's layout becomes unknown.
- Copying — requires a runtime size query.
- Calling conventions — ABIs assume fixed-size parameters.

### 7.2 The break propagates outward

An array of variable-size structs loses uniform stride; a struct containing a variable-size struct loses uniform stride; and so on up the containment chain.

### 7.3 Conclusion

Baking the size into the type (`Array<'T, n>`) is the mechanism that guarantees every value of a given type is the same number of bytes. That guarantee is what makes indexing, copying, embedding, and calling all cheap.

---

## 8. The `Array<'T, n>` Storage Primitive

### 8.1 Compiler-provided layout

`Array<'T, n>` is a compiler-provided storage primitive: `n` contiguous elements of type `'T`. Its byte size is `n * sizeof('T)`. It has no header. Both parameters may be supplied as concrete arguments (`Array<Int, 10000>`), forwarded from an enclosing scope (`Array<'T, n>`), or inferred by a constructor from a literal (`Array([1, 2, 3])`).

### 8.2 Array is the fixed-size storage base case

Other fixed-size containers (vectors, matrices) are defined in terms of `Array` and need no extra compiler support. Dynamic container types are not specified here; when specified, they are separate runtime-managed wrappers over opaque `@primitives$...` storage, not extensions of `Array`.

---

## 9. Deferred Features

The following are intentionally not specified in this version:

- arithmetic on number parameters in type positions (for example `Array<'T, rows * cols>`), pending a type-level equality rule for such expressions
- dynamic container types such as lists and maps
- bounds-checking rules for element access APIs
- named lane access (`.x`, `.y`, `.z`, `.w`)
- phantom type parameters — a parameter with no path from any argument, return, receiver, or literal that fixes it

---

## 10. Design Rationale

| Decision | Rationale |
|---|---|
| Types are templated functions | Treating a type as something that is executed makes templating a first-class consequence rather than a separate feature. A type definition is a function from parameters to a layout. |
| One unified parameter system | A single positional `<>` slot system for both type and number parameters removes the old split between apostrophe generics and bracketed size parameters. The marker, not the position, carries the kind. |
| `<>` for type expressions, `()` for constructor calls | `<>` belongs to the type system and describes architecture at compile time; `()` belongs to the value system and constructs at run time. Keeping them separate keeps each mechanism simple. |
| No `<>` at constructor calls | A constructor is a function called by name. Type information reaches it by inference from values or by being passed as an argument, so a parallel `<>` channel at the call site is unnecessary and is disallowed. |
| Type parameter written `'T`, number parameter lowercase | The casing and the apostrophe encode the kind directly, so the reader and the parser both know what a slot holds without a separate declaration. |
| Size is part of the type | Fixed size guarantees uniform stride, which is what makes indexing, copying, struct embedding, and calling conventions cheap. Runtime-sized values would propagate stride loss up every containment chain. |
| Concept-typed literals must be wrapped | The compiler cannot pick a concrete numeric type from a bare literal. One explicit `Int(...)` wrap at the call site replaces a `<>` argument list everywhere else. |
| `Array<'T, n>` is the single storage primitive | One fixed-size base case keeps the compiler's layout responsibility minimal; every other fixed-size container is defined in terms of it. |
| Number parameters resolve to values in body positions | The same symbol that fixes a layout size is readable as an ordinary number inside a method body, so size logic needs no parallel accessor. |

---

## 11. Summary

| Concept | Rule |
|---|---|
| Type as template | A type definition declares parameters in a `<>` header and produces a layout; applying arguments evaluates it into a concrete type |
| Type parameter | A `'`-prefixed uppercase name (`'T`); ranges over types |
| Number parameter | A lowercase name (`n`); ranges over compile-time numbers and resolves to a number value in body positions |
| Unified slots | Both kinds occupy the same positional `<>` slots and constructor parameter list; the marker carries the kind |
| Type expression | `Type<arg, ...>`; a compile-time structural description; used in fields, signatures, returns, aliases, and nested arguments |
| Constructor call | `Type(arg, ...)`; a runtime construction; always by bare name; never takes a `<>` list |
| Type into a constructor | Passed as an argument to a `type` parameter (`Vector(Int)`) or inferred from value arguments (`Vector(Int(2), Int(3))`) |
| Number into a constructor | Passed as an argument to a `number` parameter (`Array(Int, 10000)`) |
| Concept-typed literal | Must be wrapped in its destination type before driving inference |
| `Array<'T, n>` | Compiler-provided fixed-size storage primitive: `n` contiguous elements of type `'T` |
| Size in the type | Required for uniform stride and therefore for cheap indexing, copying, embedding, and calls |
