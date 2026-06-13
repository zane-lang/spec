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
name (ParamType, ...) -> ReturnType(param Name Type, ...) { body }
```

`Type{fieldA, fieldB}` is shorthand for `Type{fieldA: fieldA, fieldB: fieldB}`.

The last form is the lambda-variable shorthand: a fusion of a variable name, a function-type ascription, and a lambda body header in the shape of a function or method declaration. The function-type ascription uses the **old** `(T) -> R` form on the ascription side; the body header is the same parenthesized declaration-form parameter list used by free-function and method declarations in §3.1 and §3.2. `this` is legal only in the first body-header position; `mut` is legal only when the first body-header parameter is `this`. The body is a normal body.

```zane
callback (Int) -> Float(x Int) {
    return Float(x.value) * 2
}
```

is shorthand for

```zane
callback (Int) -> Float = (x) {
    return Float(x.value) * 2
}
```

A mutating-method lambda variable carries the `mut` marker on the function-type ascription (in the old form, `mut` lives inside the ascription's parentheses, before the `->`; the new bracket form for the same type is `Float[this Player] mut` per §2.9):

```zane
callback (this Player) mut -> Float(this Player) {
    this.shooting = false
}
```

is shorthand for

```zane
callback (this Player) mut -> Float = (this) mut {
    this.shooting = false
}
```

The variable's type is fixed at first declaration. Once `callback` is declared as `(Int) -> Float`, the ascription side of the declaration never changes; only the body is reassigned through `callback = (...) { ... }`. The shorthand is recognized only when a function-type ascription follows the variable name. Without an ascription, the parser cannot distinguish the construct from a function declaration.

> **See also:** [`functions.md`](functions.md) §7 for the contextual-typing rule and the no-capture rule that still apply to the underlying lambda literal. §2.9 for the function-type ascription syntax. §3.8 for the underlying lambda-literal form.

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
ReturnType[]
ReturnType[ParamType, ...]
ReturnType[&ParamType, ...]
ReturnType[this ReceiverType, ...]
ReturnType[this ReceiverType, ...] mut
&ReturnType[&ParamType, ...]
ReturnType[ParamType, ...] ? AbortType
```

The shape is `ReturnType [paramTypes] mut? ?AbortType?`. The brackets are required even when the parameter list is empty, so `Void[]` is the function type of a parameterless free function. `mut` is legal only when the first parameter is `this`; it is written after the brackets, never inside them. The `? AbortType` abortable-return marker keeps its position after the brackets.

Methods that bind type-parameter symbols in their `this` type (see [`generics.md`](generics.md) §4.3) nest the receiver's type-parameter slot directly before the function-type parameter list:

```zane
Array[n] rowAt(this Buffer[n], i Int)   // function type: Array[n][this Buffer[n], i Int]
```

The shape `Array[n][this Buffer[n], i Int]` is legal under the type-parameter adjacency rule from [`generics.md`](generics.md) §2.4: a type-parameter slot contains a *name* (a binder or a reference), while a function-type parameter list contains a `this` keyword, a type name, or an `&` prefix. The lexer disambiguates the two by what the bracket contains. The *bracket-content delimiter* convention is: **a bracket that contains `this`, a type name, or `&` is a function-type parameter list; a bracket that contains a bare name is a type-parameter slot**. Adjacent type-parameter slots continue to require a non-type-parameter delimiter (an uppercase letter or another non-name character).

> **See also:** [`generics.md`](generics.md) §2.4 for the type-parameter adjacency rule. [`functions.md`](functions.md) §7 for the lambda-variable shorthand and the explicit-typed lambda literal that use this function-type form. [`error-handling.md`](error-handling.md) §2 for the `? AbortType` abortable-return rule.

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
ReturnType(params DeclForm) { body }
ReturnType(this ReceiverType, params DeclForm) mut { body }
(name, ...) { body }
(this, name, ...) { body }
(this, name, ...) mut { body }
```

A lambda literal may write the return type and the parameter types explicitly. The explicit-typed form parallels the declaration form: the return type and parameter types are written directly, and the body header is the same parenthesized declaration-form parameter list used by free-function and method declarations. `this` is legal only in the first body-header position; `mut` is legal only when the first body-header parameter is `this`. The contextual-typing form (the last three lines) is legal only in a position with a known destination function type: a single parameter whose type is a function type, a typed variable whose type is a function type, or a return slot whose type is a function type. The contextual form relies on that destination to supply the parameter types, return type, and abort type.

The contextual form is also legal as the body of the lambda-variable shorthand (see §1.1) and as the right-hand side of `name = (params) { body }` for a function-typed variable.

Examples:

```zane
element!onClick((eventData) {
    ...
})
```

is the contextual-typing form; the surrounding `onClick` parameter type supplies the function type. The same literal with explicit types reads:

```zane
element!onClick(Void(eventData EventData) {
    ...
})
```

A mutating-method lambda with an explicit `this` type:

```zane
element!onClick(Void(this Widget, data EventData) mut {
    ...
})
```

In the mutating-method literal, the `mut` keyword sits between the body header and the body block; it marks the underlying function type as a mutating-method type (`Void[this Widget, data EventData] mut` per §2.9). The body header inside the parentheses uses declaration form: a `this` parameter named with its receiver type, then any additional parameters with their names and types.

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

`PackageName$name` is not a value in Zane. The only way to produce a function value is a lambda literal (see §3.8) or a lambda variable (see §1.1).

> **See also:** [`functions.md`](functions.md) §4.4 for the rule that function names are grammar, not values. §7 for the lambda forms that produce function values.

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
