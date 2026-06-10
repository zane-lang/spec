# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. Topic documents define semantics; this document defines form only.

> **See also:** [`types.md`](types.md) for constructors. [`functions.md`](functions.md) for methods. [`control-flow.md`](control-flow.md) for branching and loop semantics. [`error-handling.md`](error-handling.md) for abort semantics. [`operators.md`](operators.md) for precedence.

---

## 1. Declarations

### 1.1 Symbols

New symbol declarations:

```zane
name Type(args, ...)
name Type{field: expr, ...}
name Type{fieldA, fieldB, ...}
name Type = expr
name &Type = expr
```

`Type{fieldA, fieldB}` is shorthand for `Type{fieldA: fieldA, fieldB: fieldB}`.

Every symbol declaration is directly initialized. Bare forms such as `name Type` and `name &Type` are not declaration forms.

```zane
name Type   // ILLEGAL: symbols require direct initialization
```

Once a symbol already exists, reassignment uses only:

```zane
name = expr
```

### 1.2 Package constants

```zane
name Type(value)
```

### 1.3 Class fields

```zane
class Name {
    field Type
    field &Type
}
```

### 1.4 Struct fields

```zane
struct Name {
    field Type
}
```

### 1.5 Imports

```zane
import PackageName
```

---

## 2. Types

### 2.1 Core surface types
`Int`, `Float`, `Bool`, `String`, `Void`

These are public language-level types, not storage primitives. `Int`, `Float`, and `Bool` are nominal wrapper structs over machine storage primitives in the `@primitives$` namespace. `String` and other runtime-managed core types use the same wrapper pattern over opaque runtime primitives. `Void` is the exception: it is a core surface type with no storage payload.

### 2.2 Named types

```zane
PackageName$Type
Type
```

### 2.3 Reference types

```zane
&Type
```

`&Type` is legal in storage sites (local-variable declarations, fields, and nested storage types such as the example below), as well as in function and constructor parameter positions and return-type positions.

```zane
Array[size] of &Node
```

### 2.4 Inferred type generics

A type generic is introduced by a `'`-prefixed name in a type position inside a declaration body. There is no separate binder syntax at the declaration header.

```zane
struct Box {
    value 'T
}
```

The set of unique `'`-prefixed names in the body is the named type-generic set of the declaration. The compiler infers the type-generic set at use sites from call-argument types and type ascriptions. Callers never write type arguments; see [`generics.md`](generics.md) §5.1 for the rule.

### 2.5 Type-parameterized types

Definition-site type-parameter binders:

```zane
struct Matrix[rows]X[cols] {
    ...
}
```

Use-site form (type parameters are baked into the type name; the type generics of the body are inferred):

```zane
Matrix10X20
```

Digits are illegal in identifiers except where they supply type parameters to a type-parameterized type name.

### 2.6 Array storage primitive

```zane
Array[size]
```

`Array[size]` is a compiler-provided storage primitive representing `size` contiguous elements of an inferred type. The element type is a type generic and is inferred from the surrounding context, just like any other type generic in the language.

### 2.7 Reserved compiler namespaces

```zane
@primitives$name
@concepts$name
```

The `@primitives$` namespace contains storage primitives such as machine-word scalar types and opaque runtime primitives used by core wrapper types. The `@concepts$` namespace contains compiler concept types used for source literals.

### 2.8 Compiler concept types for literals

```zane
@concepts$Number
@concepts$Text
@concepts$Tuple
@concepts$Collection
```

These compiler-provided concept types represent source literals before they are lowered into storage types. Concept types may appear in parameter positions but **MUST NOT** be used as storage types such as local variables, fields, or nested storage positions. Functions and constructors may use concept-typed parameters to accept literals and lower them into the corresponding core surface type.

### 2.9 Function types

```zane
(ParamType, ...) -> ReturnType
(ParamType, ...) -> ReturnType ? AbortType
(this ReceiverType, ParamType, ...) -> ReturnType
(this ReceiverType, ParamType, ...) mut -> ReturnType
(this ReceiverType, ParamType, ...) -> &ReturnType
(this ReceiverType, ParamType, ...) -> ReturnType ? AbortType
(this ReceiverType, ParamType, ...) mut -> ReturnType ? AbortType
```

Reference-typed parameters and returns use the ordinary type form:

```zane
(&ParamType, ...) -> ReturnType
(this ReceiverType, &ParamType, ...) -> &ReturnType
```

`mut` is legal only when the first parameter is `this`.

---

## 3. Functions, Methods, Constructors, and Lambdas

### 3.1 Free functions

```zane
ReturnType name(param Type, ...) { body }
ReturnType name(param &Type, ...) { body }
ReturnType ? AbortType name(param Type, ...) { body }
ReturnType name(param Type, ...) => expr
ReturnType name(param &Type, ...) => expr
ReturnType ? AbortType name(param Type, ...) => expr
```

### 3.2 Methods

```zane
ReturnType name(this ReceiverType, param Type, ...) { body }
ReturnType name(this ReceiverType, param &Type, ...) { body }
ReturnType name(this ReceiverType, param Type, ...) mut { body }
ReturnType name(this ReceiverType, param &Type, ...) mut { body }
ReturnType ? AbortType name(this ReceiverType, param Type, ...) { body }
ReturnType ? AbortType name(this ReceiverType, param Type, ...) mut { body }
ReturnType name(this ReceiverType, param Type, ...) => expr
ReturnType name(this ReceiverType, param &Type, ...) => expr
ReturnType name(this ReceiverType, param Type, ...) mut => expr
ReturnType name(this ReceiverType, param &Type, ...) mut => expr
ReturnType ? AbortType name(this ReceiverType, param Type, ...) => expr
ReturnType ? AbortType name(this ReceiverType, param Type, ...) mut => expr
```

`this` is legal only in the first parameter position. A declaration is a method if and only if its first parameter is named `this`.

`=> expr` is legal only when the declared return type is not `Void`.

### 3.3 Positional constructors

```zane
TypeName(param Type, ...) {
    return init{ field: expr, ... }
}
TypeName(param &Type, ...) {
    return init{ field: expr, ... }
}
TypeName(param Type, ...) => init{ field: expr, ... }
TypeName(param &Type, ...) => init{ field: expr, ... }
```

Constructors use the same package-scope declaration shapes as other functions, except that the written type name is the return type and the body constructs the value with `init{ ... }`.

### 3.4 Field constructors

```zane
TypeName{
    fieldA Type,
    fieldB Type(args...),
    fieldC Type = expr,
    ...
} {
    return init{fieldA, fieldB, fieldC}
}
TypeName{
    fieldA Type,
    fieldB Type(args...),
    fieldC Type = expr,
    ...
} => init{fieldA, fieldB, fieldC}
```

Each field entry uses either the bare required-field form `field Type` or an initialized storage form such as `field Type = expr`. This is a constructor-header-specific exception to the symbol-declaration rule above: the bare form declares a required constructor input that the call site must supply, not a standalone symbol declaration with its own storage.

Field-constructor call sites may use explicit or implicit field names:

```zane
name TypeName{fieldA: expr, fieldB: expr}
name TypeName{fieldA, fieldB}
```

A field-constructor call may omit any field whose constructor entry includes an initializer.

### 3.5 Implicit constructors

```zane
implicit TypeName(param Type) {
    return init{ field: expr, ... }
}
implicit TypeName(param Type) => init{ field: expr, ... }
```

Implicit constructors use the `implicit` modifier and are written only in positional form with exactly one parameter.

Illegal forms:

```zane
implicit TypeName() { ... }           // ILLEGAL: exactly one parameter is required
implicit TypeName(a A, b B) { ... }   // ILLEGAL: implicit constructors are single-parameter only
implicit TypeName{field Type} { ... } // ILLEGAL: field-constructor form is not allowed
```

### 3.6 Subscript definitions

```zane
(this ReceiverType)[param ParamType, ...] => placeExpr
```

Subscript definitions have no explicit return type annotation. The body **MUST** be a place expression. If the body is not a place expression, the declaration is a compile-time error.

A subscript definition may declare any number of comma-separated parameters inside `[]`. The surface form is not limited to one or two parameters.

The following forms are not part of the grammar:

```zane
ReturnType (this ReceiverType)[index ParamType] => expr
```

`[]` is not a general function call form. A subscript definition always declares a place projection that references existing storage within the receiver.

### 3.7 `init{ }`

```zane
init{
    field: expr,
    otherField,
    ...
}
```

A bare field name inside `init{ }` is shorthand for `fieldName: fieldName`.

### 3.8 Lambda literals

```zane
() { body }
(name, ...) { body }
() => expr
(name, ...) => expr
(this) { body }
(this) mut { body }
(this, name, ...) { body }
(this, name, ...) mut { body }
(this) => expr
(this) mut => expr
(this, name, ...) => expr
(this, name, ...) mut => expr
```

Lambda literals omit the function name, parameter types, return type, and abort type. `this` is legal only in the first parameter position. `mut` is legal only when the first parameter is `this`.

Examples:

```zane
element!onClick((eventData) {
    ...
})

element!onClick((this, data) mut {
    ...
})
```

### 3.9 Operator definitions

```zane
ReturnType ~(value ParamType) { body }
ReturnType *(leftParam LeftType, rightParam RightType) { body }
ReturnType /(leftParam LeftType, rightParam RightType) { body }
ReturnType +(leftParam LeftType, rightParam RightType) { body }
Bool ==(leftParam LeftType, rightParam RightType) { body }
Bool <(leftParam LeftType, rightParam RightType) { body }
```

Operator definitions are package-scope function declarations whose names are operator tokens. They never declare `this`, so they are not methods and cannot use `mut`.

---

## 4. Calls and Function Values

### 4.1 Free-function calls

```zane
name(args...)
PackageName$name(args...)
```

### 4.2 Method calls

```zane
receiver:method(args...)
receiver!method(args...)
receiver:PackageName$method(args...)
receiver!PackageName$method(args...)
```

### 4.3 Function references

```zane
PackageName$functionName
```

### 4.4 Pipe syntax

```zane
callableExpr|expr
```

The left-hand side of a pipe must be a callable expression, such as a function, constructor, or method target. The right-hand side may be any expression value. Pipe has lower precedence than unary `~` (binds less tightly) and higher precedence than `*`, `/`, `+`, `-`, and the comparison operators (binds more tightly).

Examples of grouping:

```zane
someFunc|~3      // groups as someFunc|(~3)
someFunc|3 + 3   // groups as (someFunc|3) + 3
Vec2(2)|100      // groups as Vec2(2)|100
```

### 4.5 `spawn`

```zane
spawn PackageName$fn(args...)
spawn PackageName$fn(args...) ? binder { ... }
spawn PackageName$fn(args...) ? { ... }
spawn PackageName$fn(args...) ?? fallbackExpr
name Type = spawn PackageName$fn(args...)
name Type = spawn PackageName$fn(args...) ? binder { ... }
name Type = spawn PackageName$fn(args...) ? { ... }
name Type = spawn PackageName$fn(args...) ?? fallbackExpr
```

`spawn` is legal only on function-call expressions.

### 4.6 Subscript expressions

```zane
placeExpr[argExpr, ...]
```

`[]` is legal only when the receiver type defines a subscript declaration. A subscript expression is a place projection, not a general function call, so it is legal only when its base is a place expression.

Examples:

```zane
list[i]
matrix[row, col]
tensor[x, y, z]
```

`CustomList()[1]` is not a valid place expression because the base is a temporary.

### 4.7 Parenthesized expressions

```zane
(expr)
```

Parentheses group an inner expression explicitly. See [`operators.md`](operators.md) §3 for precedence.

Example:

```zane
number Int = (3 + 2) * 2
```

---

## 5. Control Flow

### 5.1 `if` / `elif` / `else`

```zane
if conditionExpr { ... }
if conditionExpr { ... } elif conditionExpr { ... }
if conditionExpr { ... } else { ... }
if conditionExpr { ... } elif conditionExpr { ... } else { ... }
```

An `if` chain may contain zero or more `elif` branches followed by an optional `else` branch.

### 5.2 `guard`

```zane
guard conditionExpr
guard conditionExpr { ... }
```

### 5.3 `loop`

```zane
loop name from startExpr to endExpr { ... }
loop name to endExpr { ... }
```

---

## 6. Error Handling

### 6.1 Abortable return types

```zane
ReturnType ? AbortType
```

### 6.2 `?` handlers

```zane
expr ? binder { ... }
expr ? { ... }
```

Every path inside the handler must end with one of:

```zane
resolve expr
resolve
return expr
abort expr
abort
```

### 6.3 `??` shorthand

```zane
expr ?? fallbackExpr
```

---

## 7. Operators and Keywords

### 7.1 Operators
`~`, `*`, `/`, `+`, `-`, `<`, `>`, `<=`, `>=`, `==`, `~=`

### 7.2 Boolean keywords
`and`, `or`

### 7.3 Control-flow keywords
`if`, `elif`, `else`, `guard`, `loop`, `from`, `to`

### 7.4 Comments

```zane
// single-line comment
/// doc comment
```

Zane has no block-comment syntax. `//` starts a single-line comment. `///` starts a documentation comment line. Adjacent `///` lines are merged into one documentation block.

---

## 8. Packages

### 8.1 Package member syntax

```zane
PackageName$member
```

### 8.2 Package declarations

```zane
package PackageName
```
