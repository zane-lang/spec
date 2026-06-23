# Zane Generics and Type Parameters

This document specifies Zane's type-parameter system. A type in Zane is a *templated function*: it takes parameters and is executed to produce a concrete layout. A **type definition** declares its parameters in a `<>` header placed after the name (`Vector<T Type>`), because a type is applied positionally (`Vector<Int>`) and that ordered header is its architecture. A **verb** — a function, method, or constructor — has no header: it *introduces* its type and number parameters inline, at their first marked occurrence in its signature (`x T Type`, `this Buffer<T Type, n Number>`), and references them by bare name thereafter. Type arguments are written in `<>` type expressions; they are never written at a constructor or function call.

> **See also:** [`syntax.md`](syntax.md) §2 for the surface syntax of type expressions. [`types.md`](types.md) §5 for `type` and `alias` declarations and §3 for constructors. [`lexical.md`](lexical.md) §3 for the casing rule that distinguishes types from values. [`functions.md`](functions.md) §5 for the generic-match phase of overload resolution.

---

## 1. Overview

Zane treats a type as something that is *executed*. A type definition takes parameters and produces a concrete layout, the same way a function takes parameters and produces a value. Templating is therefore not a bolt-on feature; it falls directly out of types being executable, and out of types themselves being ordinary compile-time values.

- **`Types are templated functions`.** A parameterized *type* lists its parameters in a `<>` header and produces a result. Applying arguments to those parameters (`Vector<Int>`) evaluates the template into a concrete type.
- **`Types use a header; verbs introduce inline`.** A type's parameters are applied positionally, so a type keeps its `<>` header — an ordered, explicit signature. A verb's parameters are always inferred and never applied positionally, so a verb has no header: it introduces each parameter inline at its first marked occurrence (`x T Type`, `Array<T Type, n Number>`) and references it bare elsewhere.
- **`Parameters are concept-typed`.** A parameter is declared `name Type` (a type parameter) or `name Number` (a number parameter). `Type` and `Number` are compiler concept types; the parameter's casing follows what it names — `T` is a type, `n` is a number.
- **`References are bare`.** Inside a body or a nested type, a parameter is referenced by its bare name (`T`, `n`). There is no sigil: a name is *introduced* once — by a type's header or by a verb's first inline occurrence carrying its concept — and the casing rule keeps the two kinds distinct.
- **`<>` describes architecture, `()` constructs values`.** A `<>` type expression is a compile-time description that lives in the type system. A `()` call is a runtime construction that lives in the value system. They are different mechanisms, not two syntaxes for one idea.
- **`No type arguments at calls`.** A constructor or function is always called by its bare name with `()`. Type and number parameters reach it either inferred from the value arguments (inline-introduced parameters) or passed as ordinary arguments (`Type`/`Number` value parameters).
- **`Array is the storage primitive`.** `Array<T, n>` is the single compiler-provided fixed-size storage primitive: `n` contiguous elements of type `T`.

---

## 2. Types as Templated Functions

### 2.1 A type definition takes parameters

A parameterized type declares its parameters in a `<>` header, then produces a layout. The header is the type's *signature*; the right-hand side is what it produces.

```zane
type Vector<T Type> = struct {
    x T;
    y T;
}
```

`Vector` is a template with one type parameter, `T`. It is not yet a concrete type. Applying an argument evaluates it:

```zane
Vector<Int>     // evaluates the template with T = Int
Vector<Float>   // evaluates the template with T = Float
```

Each distinct application is a distinct concrete type with a known, fixed layout.

### 2.2 Parameters may be types or numbers

A header may mix type parameters and number parameters. A type parameter is declared with the `Type` concept; a number parameter with the `Number` concept.

```zane
type Buffer<T Type, n Number> = struct {
    data Array<T, n>;
}

Buffer<Int, 64>   // T = Int, n = 64
```

`T` ranges over types; `n` ranges over compile-time numbers. Inside the body, `n` may also be read as a number value — for example, to compute a length — exactly as `data Array<T, n>` uses it to fix the storage size.

### 2.3 Concrete (non-parameterized) types

A type with no parameters needs no `<>` header. Its `<>`-free name is already a complete type.

```zane
type Wrapper = struct {
    vec Vector<Int>;
    arr Array<Int, 10000>;
}
```

`Wrapper` is concrete: every field type is fully applied, so `Wrapper`'s layout is known. A named class or struct is always declared with `type Name = class { ... }` or `type Name = struct { ... }` (see [`types.md`](types.md) §2); the `struct { ... }` and `class { ... }` bodies are type expressions and do not name a type on their own.

---

## 3. The Parameter System

### 3.1 Where parameters are introduced

A type and a verb introduce their parameters in different places, for one concrete reason: a type is applied positionally, a verb is not.

A **type definition** declares its parameters in a `<>` header placed immediately after the name. The header is the type's signature — it fixes the parameters and their order, which is exactly what a use site fills positionally (`Vector<Int>`, `Buffer<Int, 64>`).

```zane
type Vector<T Type> = struct { ... }             // header: one type parameter
type Buffer<T Type, n Number> = struct { ... }   // header: a type then a number
```

A **verb** — a function, method, or constructor — has no header. It *introduces* each type or number parameter inline, at the parameter's first **marked** occurrence — the first place the name carries its concept (`Type` / `Number`):

```zane
Vector<T>(x T Type, y T Type) { ... }             // T introduced on a value parameter
T head(arr Array<T Type, n Number>) { ... }       // T, n introduced inside a nested type
Int size(this Buffer<T Type, n Number>) { ... }   // T, n introduced on the receiver
```

A verb has no header because it never needs one: its parameters are always inferred (§5) and never applied positionally, so there is no order to fix and nothing for a header to declare.

A name is introduced by its first **marked** occurrence and referenced bare everywhere else in the signature. A bare reference may appear *before* the introduction: in `T head(arr Array<T Type, n Number>)` the return type `T` is a bare reference even though it is written first, because the introduction is the marked `T Type` in the parameter list. Within one verb signature every occurrence of a name is the **same** parameter, so `T add(x T Type, y T Type)` constrains `x` and `y` to a single type `T`; a non-introducing occurrence may be written bare (`T`) or may repeat the concept (`T Type`), and both denote the same parameter.

This is what lets a bare `T` be read unambiguously. In a **type**, a name in the enclosing header is a parameter. In a **verb**, a name introduced (marked with its concept) anywhere in the signature is a parameter for that whole signature. A name that is never introduced is a concrete type.

### 3.2 `Type` and `Number` are concept types

`Type` and `Number` are compiler-provided concept types. `Type` is the concept of a type; `Number` is the concept of a compile-time number. Like every concept type, they may appear only in parameter positions and **MUST NOT** be used as storage (see [`syntax.md`](syntax.md) §2.8). A value of concept type `Type` is a type; a value of concept type `Number` is a compile-time number. Both are available at compile time and may be used in the positions their kind allows — a `Type` value in a type position, a `Number` value in a number position.

### 3.3 References are bare; casing carries the kind

A parameter is referenced by its bare name. The casing rule (see [`lexical.md`](lexical.md) §3) carries the kind: an uppercase name is a type, a lowercase name is a number. Within one declaration, two occurrences of `T` are the same parameter, and two different names (`T` and `U`) are independent.

```zane
type Pair<T Type, U Type> = struct {
    first T;
    second U;
}
```

`first` and `second` are independent; a use site may apply different types to each (`Pair<Int, Float>`).

### 3.4 Number parameters resolve to values in body positions

A number parameter referenced in a body position (not a type position) resolves to its number value. This is how a method on a parameterized type can read a size as an ordinary number.

```zane
Int size(this Buffer<T Type, n Number>) {
    return n
}
```

Here `n` in the return position is the number the use site supplied for that parameter. The receiver type `Buffer<T Type, n Number>` introduces `T` and `n` inline; the return position references `n`. The `Array<T, n>` layout inside `Buffer` uses the same `n` to fix the storage size.

> **See also:** [`effects.md`](effects.md) §2 — a number parameter read in a body position is a read-only value-like binding.

---

## 4. Type Expressions (`<>`)

### 4.1 `<>` is the application syntax of the type system

A type expression applies arguments to a parameterized type. It is a compile-time, structural description — it says what a value's architecture *is*, it does not build a value. Type expressions appear wherever a type is expected:

```zane
type Wrapper = struct {
    vec Vector<Int>;
    arr Array<Int, 10000>;
}

Vec2 transform(this Vec2, mat Matrix<Float, 3>) { ... }
```

Struct fields, function and method signatures, return types, aliases, and nested arguments are all type-expression positions.

### 4.2 Arguments are positional

Arguments fill the header's parameters left to right. A type argument fills a `Type` slot; a number argument fills a `Number` slot.

```zane
Array<Int, 10000>     // T = Int, n = 10000
Matrix<Float, 3>      // type then number
```

A type argument may itself reference a parameter that the surrounding scope has in scope. Inside a declaration whose header binds `T` and `n`, the expression `Array<T, n>` forwards both:

```zane
type Buffer<T Type, n Number> = struct {
    data Array<T, n>;
}
```

### 4.3 Casing makes `<>` unambiguous

The token before `<` in a type expression is always a type, which is always uppercase-initial (see [`lexical.md`](lexical.md) §3). A comparison operator never has a type on its left. This is why `Vector<Int>` parses as a type application while `a < b` parses as a comparison: the casing of the left operand decides. Case-sensitive parsing is what makes the `<>` type syntax viable.

### 4.4 Inside a verb, a `<>` entry may introduce a parameter

A type expression normally *applies* arguments. Inside a verb signature — where there is no header — the same `<>` brackets are also where a nested type or number parameter is introduced: an entry that carries its concept (`T Type`, `n Number`) introduces a parameter; a bare or concrete entry references or applies as usual.

```zane
Int size(this Buffer<T Type, n Number>) => n   // legal: T and n introduced here
T first(arr Array<T, n>) { ... }               // ILLEGAL in a verb: nothing introduces T or n,
                                               // so the bare names are undefined
```

So a `<>` list in a verb may mix three kinds of entry: an *introduction* (`T Type`), a *reference* to a parameter already introduced in the same signature (`T`), and a *concrete argument* (`Int`). A type definition's header holds only introductions; a type expression outside any verb — a field, a return type of a non-generic verb, an alias — holds only references and concrete arguments.

---

## 5. Constructor and Function Calls (`()`)

### 5.1 `()` calls; a call never takes a `<>` list

A constructor call builds a value at run time; a function call runs a function. Both are invoked by bare name, and **MUST NOT** carry a `<>` list — type arguments belong to type expressions only.

```zane
vec Vector(Int(2), Int(3))   // legal: bare-name constructor call
vec Vector<Int>(Int(2))      // ILLEGAL: a call takes no <> list
```

A type or number parameter reaches a callable in one of two ways: inferred (introduced inline on a parameter's type or in a nested type) or passed explicitly (declared as a `Type`/`Number` value parameter).

### 5.2 Inferred parameters (inline)

A type or number parameter introduced inline in a verb's signature is inferred from the value arguments. The caller writes no `<>`; the compiler deduces each parameter from the argument types.

```zane
Vector<T>(x T Type, y T Type) {  // T introduced inline; x and y share it
    return init{ x, y }
}

vec Vector(Int(2), Int(3))       // T inferred as Int from the arguments
```

A constructor's name is its return type, so a constructor for a parameterized type names the **applied** type (`Vector<T>`), where the `<...>` holds bare *references* to the inline-introduced parameters — it carries `T`, not `T Type`, so it is a type expression, not a reintroduced header. The call is still by bare name (`Vector(Int(2), Int(3))`); only the declaration shows the applied return type.

### 5.3 Explicit parameters (`Type` / `Number` value parameters)

A type or number can instead be passed as an ordinary argument by declaring a value parameter of concept type `Type` or `Number`. The argument is then written positionally in `()`, like any other value.

The distinction from §5.2 is purely structural, read off the parameter's shape. An *inferred* parameter rides on a value parameter's **type** (`x T Type` — a value `x` whose type is the fresh parameter `T`) or in a nested type, so a value argument fixes it. An *explicit* parameter **is** the value parameter (`T Type` — a value parameter named `T` of concept `Type`), so the caller passes it directly. The first carries a value name before the concept-typed type; the second does not.

```zane
Vector<T>(T Type) {
    return init{ x = T(0), y = T(0) }
}

Array<T, n>(T Type, n Number) {
    // zero-initialise n elements of type T
}

vec Vector(Int)              // Int passed as the type argument
arr Array(Int, 10000)        // Int passed as the type, 10000 as the size
```

A `Type` value parameter is usable as a type inside the body (for example, `T(0)`); a `Number` value parameter is usable as a number. This is the practical payoff of types being compile-time values: a type handed to a constructor is just an argument the body can execute.

### 5.4 Concept-typed literals must be wrapped

A bare source literal carries a compiler concept type (such as `@concepts$Number`), not a concrete storage type. A bare literal **MUST NOT** drive inference of a type parameter, because the compiler cannot choose between `Int`, `Float`, and other concrete types. Wrap the literal in its destination type:

```zane
vec Vector(Int(2), Int(3))   // legal: each argument is a concrete Int
vec Vector(2, 3)             // ILLEGAL: literals cannot drive inference of T
```

This single explicit wrap at the call site is the deliberate cost that replaces a `<>` type-argument list at every call.

---

## 6. Array Construction

`Array<T, n>` illustrates both mechanisms together. The element type and the size are both parameters; a constructor supplies them by inference or by explicit argument.

### 6.1 Inferred from a literal

```zane
Array<T, n>(values Array<T Type, n Number>) {
    // T and n inferred from the literal
}

arr Array([Int(1), Int(2), Int(3)])         // T = Int and n = 3 inferred from the literal
```

The value-parameter type `Array<T Type, n Number>` introduces `T` and `n` inline and lets the compiler read both from the literal's element type and length.

### 6.2 Explicit type and size

```zane
Array<T, n>(T Type, n Number) {  // called as Array(Int, 10000)
    // zero-initialise n elements of type T
}

arr Array(Int, 10000)        // T = Int passed, n = 10000 passed
```

Used when no literal is available — for example, a zero-initialised array of known length.

### 6.3 Type expression

```zane
type Matrix = struct {
    data Array<Float, 9>;     // architecture: 9 contiguous Floats, stored inline
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

Baking the size into the type (`Array<T, n>`) is the mechanism that guarantees every value of a given type is the same number of bytes. That guarantee is what makes indexing, copying, embedding, and calling all cheap.

---

## 8. The `Array<T, n>` Storage Primitive

### 8.1 Compiler-provided layout

`Array<T, n>` is a compiler-provided storage primitive: `n` contiguous elements of type `T`. Its byte size is `n * sizeof(T)`. It has no header. Both parameters may be supplied as concrete arguments (`Array<Int, 10000>`), forwarded from an enclosing scope (`Array<T, n>`), or inferred by a constructor from a literal (`Array([Int(1), Int(2), Int(3)])`).

### 8.2 Array is the fixed-size storage base case

Other fixed-size containers (vectors, matrices) are defined in terms of `Array` and need no extra compiler support. Dynamic container types are not specified here; when specified, they are separate runtime-managed wrappers over opaque `@primitives$...` storage, not extensions of `Array`.

---

## 9. Deferred Features

The following are intentionally not specified in this version:

- arithmetic on number parameters in type positions (for example `Array<T, rows * cols>`), pending a type-level equality rule for such expressions
- dynamic container types such as lists and maps
- bounds-checking rules for element access APIs
- named lane access (`.x`, `.y`, `.z`, `.w`)
- phantom type parameters — an introduced parameter (a type's header parameter, or a verb's inline parameter) with no path from any value argument, receiver, or literal that fixes it
- generic function values — a function *value* that is itself polymorphic over type or number parameters; the open question is runtime representation (monomorphization versus dictionary passing), not overload resolution or type checking, since a generic function type is a unique parameter shape (see [`functions.md`](functions.md) §7.6)

---

## 10. Design Rationale

| Decision | Rationale |
|---|---|
| Types are templated functions | Treating a type as something that is executed makes templating a first-class consequence rather than a separate feature. A type definition is a function from parameters to a layout. |
| Header for types, inline introduction for verbs | A type is applied positionally (`Vector<Int>`), so its parameters' order is part of its public interface and belongs in an explicit `<>` header. A verb's parameters are always inferred and never applied positionally, so there is no order to fix and no need for a header: the verb introduces each parameter inline at its first marked occurrence and references it bare elsewhere. The split tracks the real apply-versus-infer distinction rather than imposing one form on two different roles. |
| Parameters are concept-typed (`Type` / `Number`) | A type handed to a constructor is just a compile-time value, so its parameter has a type like any other — the `Type` concept for types, `Number` for sizes. No bespoke parameter-kind keyword is needed, and `Number` and `Type` reuse the existing concept-type machinery (parameter positions only, never storage). |
| No sigil on parameters; casing carries the kind | A parameter is introduced once — by a type's header or by a verb's first inline occurrence carrying its concept — and the casing rule distinguishes a type (`T`) from a number (`n`). A bare reference is unambiguous because an introduced name is a parameter and any name never introduced is a concrete type. |
| `<>` for type expressions, `()` for calls | `<>` belongs to the type system and describes architecture at compile time; `()` belongs to the value system and constructs or runs at run time. Keeping them separate keeps each mechanism simple. |
| No `<>` at calls | A call is by name. Inline-introduced parameters are inferred from the value arguments and explicit parameters are passed as ordinary `Type`/`Number` arguments, so a parallel `<>` channel at the call site is unnecessary and is disallowed. |
| Inline parameter = inferred, value parameter = explicit | Writing a concept on a value parameter's type or in a nested type (`x T Type`, `Array<T Type, n Number>`) means "infer me"; writing it as the value parameter itself (`T Type` in a `()` list) means "pass me" — exactly the inferred-versus-explicit choice a constructor author wants. |
| Size is part of the type | Fixed size guarantees uniform stride, which is what makes indexing, copying, struct embedding, and calling conventions cheap. Runtime-sized values would propagate stride loss up every containment chain. |
| Concept-typed literals must be wrapped | The compiler cannot pick a concrete numeric type from a bare literal. One explicit `Int(...)` wrap at the call site replaces a `<>` argument list everywhere else. |
| `Array<T, n>` is the single storage primitive | One fixed-size base case keeps the compiler's layout responsibility minimal; every other fixed-size container is defined in terms of it. |

---

## 11. Summary

| Concept | Rule |
|---|---|
| Type as template | A type definition lists parameters in a `<>` header and produces a result; applying arguments evaluates a type into a concrete type |
| Type parameter | Declared `name Type` with an uppercase name (`T`); ranges over types |
| Number parameter | Declared `name Number` with a lowercase name (`n`); ranges over compile-time numbers and resolves to a number value in body positions |
| `Type` / `Number` | Compiler concept types; legal only in parameter positions, never as storage |
| Reference | A parameter is referenced by bare name; casing carries the kind. A type's header or a verb's inline concept marks a name as a parameter |
| Type expression | `Type<arg, ...>`; a compile-time structural description; used in fields, signatures, returns, aliases, and nested arguments |
| Call | `Type(arg, ...)`; a runtime construction or function call; always by bare name; never takes a `<>` list |
| Inferred parameter | Introduced inline on a verb parameter's type or in a nested type; deduced from the value arguments at the call |
| Explicit parameter | Declared as a `Type`/`Number` value parameter in `()`; passed positionally (`Vector(Int)`, `Array(Int, 10000)`) |
| Concept-typed literal | Must be wrapped in its destination type before driving inference |
| `Array<T, n>` | Compiler-provided fixed-size storage primitive: `n` contiguous elements of type `T` |
| Size in the type | Required for uniform stride and therefore for cheap indexing, copying, embedding, and calls |
