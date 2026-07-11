# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. Topic documents define semantics; this document defines form only.

> **See also:** [`types.md`](types.md) for constructors. [`functions.md`](functions.md) for methods. [`adt.md`](adt.md) for `enum`, `variant`, `match`, and enum maps. [`control-flow.md`](control-flow.md) for branching and loop semantics. [`error-handling.md`](error-handling.md) for abort semantics. [`operators.md`](operators.md) for precedence.

---

## 1. Declarations

### 1.1 Symbols

New symbol declarations:

```zane
name VarType(args, ...)
name VarType{field = expr, ...}
name VarType{fieldA, fieldB, ...}
name VarType = expr
name &VarType = expr
name ReturnType(param ParamType, ...) { body }
name ReturnType(param ParamType, ...) => expr
```

`VarType{fieldA, fieldB}` is shorthand for `VarType{fieldA = fieldA, fieldB = fieldB}`.

The last two forms declare a lambda-valued symbol. They mirror the constructor-call instantiation form `name VarType(args, ...)`: just as `text String("hello")` instantiates a value of type `String`, `callback Float(x Int) { body }` instantiates a function value. The full set of lambda-variable forms — including `this`, `mut`, and abort types — lives in §3.8.

Every symbol declaration is directly initialized. Bare forms such as `name VarType` and `name &VarType` are not declaration forms.

```zane
name VarType   // ILLEGAL: symbols require direct initialization
```

Once a symbol already exists, reassignment uses only:

```zane
name = expr
```

### 1.2 Package constants

```zane
name VarType(value)
```

### 1.3 Reference-type bodies (`#`)

A `#`-marked body declares a **reference type** — identity-bearing, may hold `&` fields, may recurse. It declares fields only and names a type through a type declaration (§1.6). There is no standalone `#struct Name { ... }` declaration form.

```zane
type Name = #struct {
    field FieldType;
    field &FieldType;
}
```

### 1.4 Value-type bodies

A value-type body (unmarked) declares a **value type** — copied, transitively value, no `&` or reference-type fields. It declares fields only and names a type through a type declaration (§1.6). There is no standalone `struct Name { ... }` declaration form.

```zane
type Name = struct {
    field FieldType;
}
```

### 1.5 Imports

```zane
import packageName
```

### 1.6 Type and alias declarations

```zane
type Name = TypeExpr
alias Name = TypeExpr
type Name<T Type, n Number> = TypeExpr
type Name = struct { field FieldType; ... }
type Name = #struct { field FieldType; ... }
type Name = variant { member FieldType; ... }
type Name = #variant { member FieldType; ... }
type Name = enum [ memberA, memberB, ... ]
```

`type` declares a new distinct named type; `alias` declares an interchangeable name. The right-hand side is any type expression (§2.4), including an inline `struct`, `#struct`, `variant`, `#variant`, or `enum` body; a leading `#` marks a reference type (§2.10). A `<>` header on the left declares the type's parameters. See [`types.md`](types.md) §5.

### 1.7 Variant declarations

A `variant` body uses the same grammar as a `struct` body: `{ }` brackets with `;`-terminated members, each a lowercase member name followed by its payload type.

```zane
type Name = variant {
    memberA TypeA;
    memberB TypeB;
}
```

> **See also:** [`adt.md`](adt.md) §3 for variant semantics.

### 1.8 Enum declarations

An `enum` body is a flat list: `[ ]` brackets with `,`-separated lowercase members. Members are payloadless.

```zane
type Name = enum [ memberA, memberB, memberC ]
```

> **See also:** [`adt.md`](adt.md) §2 for enum semantics.

### 1.9 Enum map declarations

An enum map is a package-scope declaration. It names the enum, the property, the property's type, then a `[ ]` list of `,`-separated `member = value` entries.

```zane
EnumName.property FieldType [
    memberA = valueA,
    memberB = valueB
]
```

> **See also:** [`adt.md`](adt.md) §6 for enum-map semantics.

---

## 2. Types

### 2.1 Core surface types
`Int`, `Float`, `Bool`, `String`, `Void`

These are public language-level types, not storage primitives. `Int`, `Float`, and `Bool` are nominal wrapper structs over machine storage primitives in the `@primitives$` namespace. `String` and other runtime-managed core types use the same wrapper pattern over opaque runtime primitives. `Void` is the exception: it is a core surface type with no storage payload.

### 2.2 Named types

```zane
packageName$TypeName
TypeName
```

### 2.3 Reference types

```zane
&TypeName
```

`&TypeName` is legal in storage sites (local-variable declarations, fields, and nested storage types such as the example below), as well as in function and constructor parameter positions and return-type positions.

```zane
Array<&Node, n>
```

### 2.4 Type expressions

A type expression applies arguments to a parameterized type with `<>`. Arguments are positional.

```zane
TypeName<Arg, ...>
Vector<Int>
Array<Int, 10000>
Matrix<Float, 3>
```

A type argument fills a type-parameter slot; a number argument fills a number-parameter slot. A type expression is legal in any type position: fields, parameter and return types, aliases, and nested arguments. A constructor call **MUST NOT** carry a `<>` list. Inside a verb's value parameter, a `<>` entry may also *introduce* a type or number parameter by carrying its concept (`param Array<T Type, n Number>`); see [`generics.md`](generics.md) §4.4. See [`generics.md`](generics.md) §4 and §5.

A **mould** — a `struct { ... }`, `#struct { ... }`, `variant { ... }`, `#variant { ... }`, `enum [ ... ]`, `#enum [ ... ]`, `tuple [ ... ]`, or `#tuple [ ... ]` — **MUST** appear only as the right-hand side of a `type` or `alias` declaration (§1.6); every other type position names a declared type or an instantiation (see [`types.md`](types.md) §5.3). A leading `#` marks a reference type (§2.10).

```zane
type BinOp = #struct { left &Expr; right &Expr; operator Operator; }
type QualifiedIdent = tuple[String, String];

type Expr = #variant {
    op BinOp;
    qualifiedIdent QualifiedIdent;
}
```

### 2.5 Type and number parameters

A parameterized **type** declares its parameters in a `<>` header. Each entry is `name Type` (a type parameter) or `name Number` (a number parameter). `Type` and `Number` are compiler concept types, legal only in parameter positions (§2.8).

```zane
type Vector<T Type> = struct {
    x T;
    y T;
}

type Buffer<T Type, n Number> = struct {
    data Array<T, n>;
}
```

Parameters are referenced by bare name. The casing of a name marks its kind: `T` is a type, `n` is a number. Type expressions (§2.4) supply arguments positionally at use sites.

A **verb** — a function, method, or constructor — has no `<>` header. It introduces its type and number parameters inline within its value parameters, at each parameter's first marked occurrence, by carrying the concept there (`x T Type`, `param Array<T Type, n Number>`); see §3.1 and [`generics.md`](generics.md) §3. See also [`lexical.md`](lexical.md) §3.

### 2.6 Array storage primitive

```zane
Array<T, n>
```

`Array<T, n>` is a compiler-provided storage primitive: `n` contiguous elements of type `T`. Both parameters may be concrete (`Array<Int, 10000>`), forwarded from an enclosing scope (`Array<T, n>`), or inferred by a constructor from a literal (`Array([Int(1), Int(2), Int(3)])`). See [`generics.md`](generics.md) §8.

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

The concept types `Type` and `Number` declare the type and number parameters of a parameterized declaration (see [`generics.md`](generics.md) §3). They follow the same rule: legal in parameter positions, never as storage. A `Type` parameter accepts a type; a `Number` parameter accepts a compile-time number.

### 2.9 Function types

A function type leads with its return type, then lists parameter types inside `[ ]`, then any trailing `mut`. There is no `->` arrow. This mirrors the order of function declarations (§3.1–§3.2) and lambda literals (§3.8): the return contract is written first.

```zane
ReturnType[ParamType, ...]
ReturnType?AbortType[ParamType, ...]
ReturnType[this ReceiverType, ParamType, ...]
ReturnType[this ReceiverType, ParamType, ...] mut
&ReturnType[this ReceiverType, ParamType, ...]
ReturnType?AbortType[this ReceiverType, ParamType, ...]
ReturnType?AbortType[this ReceiverType, ParamType, ...] mut
```

The abort type stays attached to the return type, exactly as in a declaration's `ReturnType?AbortType name(...)` header.

Reference-typed parameters and returns use the ordinary type form:

```zane
ReturnType[&ParamType, ...]
&ReturnType[this ReceiverType, &ParamType, ...]
```

`mut` is legal only when the first parameter is `this`.

```zane
Int[Node, Int] mut    // ILLEGAL: mut requires this as first parameter
Void[Int, this Node]  // ILLEGAL: this must be the first parameter
```

### 2.10 The `#` reference modifier

A leading `#` marks a **reference type**. It attaches only to a **mould** — `#struct { ... }`, `#variant { ... }`, `#enum [ ... ]`, or `#tuple [ ... ]` — and only as the right-hand side of a `type`/`alias` declaration (§1.6). The unmarked moulds are value types.

```zane
type Cell = #struct { value Int; }               // reference product, declared and named
type Tree = #variant { leaf Int; node &Tree; }   // reference sum
```

`&` combines with a reference type and never with a bare value type: an `&T` requires `T` to be a reference type — a declared `#struct`/`#variant`/`#enum`/`#tuple` — so a stored reference is written `&Cell` or `&Tree` (see [`memory.md`](memory.md) §2.4). See [`types.md`](types.md) §2.1 for the semantics.

---

## 3. Functions, Methods, Constructors, and Lambdas

### 3.1 Functions

```zane
ReturnType name(param ParamType, ...) { body }
ReturnType name(param &ParamType, ...) { body }
ReturnType?AbortType name(param ParamType, ...) { body }
ReturnType name(param ParamType, ...) => expr
ReturnType name(param &ParamType, ...) => expr
ReturnType?AbortType name(param ParamType, ...) => expr
ReturnType name(param T Type, ...) { body }
ReturnType name(param Container<T Type, n Number>, ...) { body }
```

A function, method, or constructor has no `<>` parameter header. It introduces a type or number parameter inline within its value parameters, at the parameter's first **marked** occurrence — on a value parameter's type (`param T Type`) or inside a value parameter's nested type (`param Container<T Type, n Number>`) — and references it bare elsewhere, including in positions written earlier such as the return type. Inline parameters are inferred from the value arguments at the call; the same `Type` / `Number` concepts are used as in a type definition's header (§2.5). See [`generics.md`](generics.md) §3 and §5.

### 3.2 Methods

```zane
ReturnType name(this ReceiverType, param ParamType, ...) { body }
ReturnType name(this ReceiverType, param &ParamType, ...) { body }
ReturnType name(this ReceiverType, param ParamType, ...) mut { body }
ReturnType name(this ReceiverType, param &ParamType, ...) mut { body }
ReturnType?AbortType name(this ReceiverType, param ParamType, ...) { body }
ReturnType?AbortType name(this ReceiverType, param ParamType, ...) mut { body }
ReturnType name(this ReceiverType, param ParamType, ...) => expr
ReturnType name(this ReceiverType, param &ParamType, ...) => expr
ReturnType name(this ReceiverType, param ParamType, ...) mut => expr
ReturnType name(this ReceiverType, param &ParamType, ...) mut => expr
ReturnType?AbortType name(this ReceiverType, param ParamType, ...) => expr
ReturnType?AbortType name(this ReceiverType, param ParamType, ...) mut => expr
ReturnType name(this ReceiverType<T Type, n Number>, param ParamType, ...) { body }
```

`this` is legal only in the first parameter position. A declaration is a method if and only if its first parameter is named `this`.

`=> expr` is legal only when the declared return type is not `Void`.

### 3.3 Positional constructors

```zane
TypeName(param ParamType, ...) {
    return init{ field = expr, ... }
}
TypeName(param &ParamType, ...) {
    return init{ field = expr, ... }
}
TypeName(param ParamType, ...) => init{ field = expr, ... }
TypeName(param &ParamType, ...) => init{ field = expr, ... }
TypeName<T>(param T Type, ...) { return init{ field = expr, ... } }
TypeName<T, n>(param Container<T Type, n Number>, ...) { return init{ field = expr, ... } }
```

Constructors use the same package-scope declaration shapes as other functions, except that the written type name is the return type and the body constructs the value with `init{ ... }`.

A constructor for a parameterized type has no `<>` header; its name carries the **applied** return type (`TypeName<T>`, `TypeName<T, n>`), whose `<...>` holds bare references to the parameters. It introduces those type and number parameters inline within its value parameters — directly (`param T Type`) or inside a parameter's nested type (`param Container<T Type, n Number>`) — in which case they are inferred from the value arguments; or it accepts a type or compile-time number as an ordinary value parameter of concept type `Type` or `Number` (passed explicitly). A constructor is always called by its bare name and **MUST NOT** carry a `<>` list at the call. See [`types.md`](types.md) §3.9 and [`generics.md`](generics.md) §5.

### 3.4 Field constructors

```zane
TypeName{
    fieldA FieldType,
    fieldB FieldType(args...),
    fieldC FieldType = expr,
    ...
} {
    return init{fieldA, fieldB, fieldC}
}
TypeName{
    fieldA FieldType,
    fieldB FieldType(args...),
    fieldC FieldType = expr,
    ...
} => init{fieldA, fieldB, fieldC}
```

Each field entry uses either the bare required-field form `field FieldType` or an initialized storage form such as `field FieldType = expr`. This is a constructor-header-specific exception to the symbol-declaration rule above: the bare form declares a required constructor input that the call site must supply, not a standalone symbol declaration with its own storage.

Field-constructor call sites may use explicit or implicit field names:

```zane
name TypeName{fieldA = expr, fieldB = expr}
name TypeName{fieldA, fieldB}
```

A field-constructor call may omit any field whose constructor entry includes an initializer.

### 3.5 Implicit constructors

```zane
implicit TypeName(param ParamType) {
    return init{ field = expr, ... }
}
implicit TypeName(param ParamType) => init{ field = expr, ... }
```

Implicit constructors use the `implicit` modifier and are written only in positional form with exactly one parameter.

Illegal forms:

```zane
implicit TypeName() { ... }           // ILLEGAL: exactly one parameter is required
implicit TypeName(a A, b B) { ... }   // ILLEGAL: implicit constructors are single-parameter only
implicit TypeName{field FieldType} { ... } // ILLEGAL: field-constructor form is not allowed
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
    field = expr,
    otherField,
    ...
}
```

A bare field name inside `init{ }` is shorthand for `fieldName = fieldName`.

### 3.8 Lambda literals and lambda-variable declarations

A lambda literal is a function declaration with the name removed. It writes its own return type, parameter types, abort type, and `mut`, exactly like a named function:

```zane
ReturnType() { body }
ReturnType(param ParamType, ...) { body }
ReturnType() => expr
ReturnType(param ParamType, ...) => expr
ReturnType?AbortType(param ParamType, ...) { body }
ReturnType(this ReceiverType) { body }
ReturnType(this ReceiverType) mut { body }
ReturnType(this ReceiverType, param ParamType, ...) { body }
ReturnType(this ReceiverType, param ParamType, ...) mut { body }
ReturnType(this ReceiverType, param ParamType, ...) => expr
ReturnType(this ReceiverType, param ParamType, ...) mut => expr
```

A lambda literal omits only the function name. `this` is legal only in the first parameter position. `mut` is legal only when the first parameter is `this`. `=> expr` is legal only when the declared return type is not `Void`.

Examples:

```zane
element!onClick(Void(eventData EventData) {
    ...
})

element!onClick(Void(this Element, data EventData) mut {
    ...
})
```

A lambda-variable declaration binds a lambda literal to a symbol. The shorthand writes the symbol name in front of the lambda literal and drops the separate `= literal`, mirroring the constructor-call instantiation form `name VarType(args, ...)`:

```zane
name ReturnType(param ParamType, ...) { body }
name ReturnType(param ParamType, ...) => expr
name ReturnType?AbortType(param ParamType, ...) { body }
name ReturnType(this ReceiverType, param ParamType, ...) mut { body }
```

The shorthand expands to a symbol declaration whose type is the function type (§2.9) and whose value is the lambda literal:

```zane
callback Void[this Player] mut = Void(this Player) mut {
    this.shooting = false
}

callback Void(this Player) mut {        // shorthand for the line above
    this.shooting = false
}
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

Operator definitions are package-scope verb declarations whose names are operator tokens. They never declare `this`, so they are not methods and cannot use `mut`.

---

## 4. Calls and Function Values

### 4.1 Function calls

```zane
name(args...)
packageName$name(args...)
```

### 4.2 Method calls

```zane
receiver:method(args...)
receiver!method(args...)
receiver:packageName$method(args...)
receiver!packageName$method(args...)
```

### 4.3 Callables are call-only

Methods, functions, and operators have no value form. A package-scope callable name may appear only in call position; it cannot be written as a bare value.

```zane
packageName$functionName(args...)   // legal: call position
packageName$functionName            // ILLEGAL: callables cannot be referenced as values
+                                   // ILLEGAL: operators cannot be referenced as values
```

To obtain a function value, declare a lambda-variable (§3.8). A lambda-variable is an ordinary symbol with a single function type, so it carries no overload set.

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
spawn packageName$fn(args...)
spawn packageName$fn(args...) ? binder { ... }
spawn packageName$fn(args...) ? { ... }
spawn packageName$fn(args...) ?? fallbackExpr
name VarType = spawn packageName$fn(args...)
name VarType = spawn packageName$fn(args...) ? binder { ... }
name VarType = spawn packageName$fn(args...) ? { ... }
name VarType = spawn packageName$fn(args...) ?? fallbackExpr
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

### 4.8 `match` expressions

A `match` expression names one or more scrutinees — a bare `,`-separated list, never parenthesised — then a `{ }` block of `;`-terminated arms. Each arm is an optional binder, a case selector, `=>`, and a body. A `match` is an expression and may carry a trailing `?` (or `??`) handler when its arms are abortable.

```zane
match scrutinee { arm; arm; ... }
match scrutinee, scrutinee { arm; arm; ... }
name VarType = match scrutinee { arm; arm; ... }
name VarType = match scrutinee { arm; arm; ... } ? binder { ... }
```

An arm is `[binder] selector => body`, with one `[binder] selector` per scrutinee position, `,`-separated in order. A selector is a single case name or a `[ ]` list of `,`-separated case names, written bare (rooted at that scrutinee's type). The body is an expression (`=> expr`) or a `{ }` block.

```zane
result String = match e {
    x strLit            => x;
    [intLit, floatLit]  => "number";
    b op                => render(b);
    // every case must be covered — see adt.md §5
}

// several scrutinees: one selector per position
newState State = match state, event {
    [idle, running], keyPress => State.running;
    running,         timeout  => State.idle;
    // every combination must be covered — see adt.md §5.6
}
```

> **See also:** [`adt.md`](adt.md) §5 for `match` semantics.

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
ReturnType?AbortType
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
packageName$member
```

### 8.2 Package declarations

```zane
package packageName
```
