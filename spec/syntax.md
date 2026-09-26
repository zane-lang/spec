# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. Topic documents define semantics; this document defines form only.

> **See also:** [`types.md`](types.md) for constructors. [`functions.md`](functions.md) for methods. [`adt.md`](adt.md) for `enum`, `variant`, `match`, and enum maps. [`control-flow.md`](control-flow.md) for branching and loop semantics. [`error-handling.md`](error-handling.md) for abort semantics. [`operators.md`](operators.md) for precedence.

---

## 1. Declarations

### 1.1 Symbols

New symbol declarations:

```zane
name VarType(args, ...)
name VarType{field = expr; ...}
name VarType{fieldA; fieldB; ...}
name VarType = expr
name &VarType = expr
name ReturnType(param ParamType, ...) { body }
name ReturnType(param ParamType, ...) => expr
```

`VarType{fieldA; fieldB;}` is shorthand for `VarType{fieldA = fieldA; fieldB = fieldB;}`.

The `VarType` position of `name VarType(args, ...)` may be a qualified `Type.member` — a **named constructor** (see [`types.md`](types.md) §3.4) or a variant **case** (see [`adt.md`](adt.md) §3.2). `v Vector2.diagonal(Float(3.0))` and `e Expr.intLit("5")` both instantiate at the base type: the declared symbol holds `Vector2` / `Expr`, never `Vector2.diagonal` or a per-case type.

The last two forms declare a lambda-valued symbol. They mirror the constructor-call instantiation form `name VarType(args, ...)`: just as `text String("hello")` instantiates a value of type `String`, `callback Float(x Int) { body }` instantiates a function value. The full set of lambda-variable forms — including `this`, `mut`, and abort types — lives in §3.8.

Every symbol declaration is directly initialized. Bare forms such as `name VarType` and `name &VarType` are not declaration forms.

```zane
name VarType   // ILLEGAL: symbols require direct initialization
```

Once a symbol already exists, reassignment uses only:

```zane
name = expr
```

> **Story:** [`stories/syntax.md`](../stories/syntax.md#the-order-assignment-forced) — "The order assignment forced".

### 1.2 Package constants

```zane
name VarType(value)
```

### 1.3 Reference-type bodies (`#`)

A `#`-marked body declares a **reference type** — identity-bearing, may hold `&` fields, moved rather than copied. It declares fields only and names a type through a type declaration (§1.6). There is no standalone `#struct Name { ... }` declaration form.

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
import packageName as alias
import packageName$member
import packageName$member as alias
import packageName$[memberA, memberB]
import packageName$
```

The trailing `$` form carries no member list and no `*`. An `as` clause is legal on the whole-package form and on a single-member form, never on the bracket list or the trailing-`$` form.

```zane
import math$[sqrt, pow] as m   // ILLEGAL: a list has no single name to rename
import math$ as m              // ILLEGAL: nothing is qualified to rename
```

> **See also:** [`packages.md`](packages.md) §3 for what each form makes available.

### 1.6 Type and alias declarations

```zane
type Name = TypeExpr
alias Name = TypeExpr
type Name<T Type, n @concepts$Int> = TypeExpr
type Name = struct { field FieldType; ... }
type Name = #struct { field FieldType; ... }
type Name = variant { member FieldType; ... }
type Name = #variant { member FieldType; ... }
type Name = enum [ memberA, memberB, ... ]
```

`type` declares a new distinct named type; `alias` declares an interchangeable name. The right-hand side is any type expression (§2.4), including an inline `struct`, `#struct`, `variant`, `#variant`, or `enum` body; a leading `#` marks a reference type (§2.14). A `<>` header on the left declares the type's parameters. See [`types.md`](types.md) §5.

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

An enum map is a package-scope declaration. It names the enum, the property, the property's type, then a `{ }` body of `;`-terminated `member = value` entries.

```zane
EnumName.property FieldType {
    memberA = valueA;
    memberB = valueB;
}
```

> **See also:** [`adt.md`](adt.md) §6 for enum-map semantics.

---

## 2. Types

### 2.1 Fundamental types

`Int`, `Float`, `Bool`, `String`, `Unit`, `Array<T, n>`, `List<T>`

These are the types the `core` package declares. They are reached through an import like any other package's members ([`packages.md`](packages.md) §3.3), so `import core` writes them `core$Int` and `import core$` writes them unqualified. See [`types.md`](types.md) §2.6 for their semantics.

### 2.2 Named types

```zane
packageName$TypeName
TypeName
```

### 2.3 Reference types

```zane
&TypeName
```

`&TypeName` is a **guest** type. It is legal in storage sites (local-variable declarations, fields, and nested storage types such as the example below), as well as in function and constructor parameter positions and return-type positions. It is the only marker a type may carry.

```zane
Array<&Node, n>
```

See [`memory.md`](memory.md) §2.9 for the semantics of the two passing modes.

### 2.4 Type expressions

A type expression applies arguments to a parameterized type with `<>`. Arguments are positional.

```zane
TypeName<Arg, ...>
Vector<Int>
Array<Int, 10000>
Matrix<Float, 3>
```

A type argument fills a type-parameter slot; a number argument fills a number-parameter slot. A type expression is legal in any type position: fields, parameter and return types, aliases, and nested arguments. A constructor call **MUST NOT** carry a `<>` list. Inside a verb's value parameter, a `<>` entry may also *introduce* a type or number parameter by carrying its concept (`param Array<T Type, n @concepts$Int>`); see [`generics.md`](generics.md) §4.4. See [`generics.md`](generics.md) §4 and §5.

A **mould** — a `struct { ... }`, `#struct { ... }`, `variant { ... }`, `#variant { ... }`, `enum [ ... ]`, or `#enum [ ... ]` — **MUST** appear only as the right-hand side of a `type` or `alias` declaration (§1.6); every other type position names a declared type or an instantiation (see [`types.md`](types.md) §5.3). A leading `#` marks a reference type (§2.14).

```zane
type Operation = #struct { left Expr; right Expr; op Operator; }
type QualifiedIdent = struct { packageName String; member String; }

type Expr = #variant {
    op Operation;
    qualifiedIdent QualifiedIdent;
}
```

### 2.5 Type and number parameters

A parameterized **type** declares its parameters in a `<>` header. Each entry is `name Type` (a type parameter) or `name @concepts$Int` (a number parameter). `Type` and `@concepts$Int` are compiler concept types, legal only in parameter positions (§2.11).

```zane
type Vector<T Type> = struct {
    x T;
    y T;
}

type Buffer<T Type, n @concepts$Int> = struct {
    data Array<T, n>;
}
```

Parameters are referenced by bare name. The casing of a name marks its kind: `T` is a type, `n` is a number. Type expressions (§2.4) supply arguments positionally at use sites.

A **verb** — a function, method, or constructor — has no `<>` header. It introduces its type and number parameters inline within its value parameters, at each parameter's first marked occurrence, by carrying the concept there (`x T Type`, `param Array<T Type, n @concepts$Int>`); see §3.1 and [`generics.md`](generics.md) §3. See also [`lexical.md`](lexical.md) §3.

### 2.6 Container storage primitives

```zane
@primitives$Array<T, n>
@primitives$List<T>
```

`@primitives$Array<T, n>` is `n` contiguous elements of type `T`, a value type; `@primitives$List<T>` is its dynamically sized counterpart, a reference type. `core` declares `Array<T, n>` and `List<T>` over them (§2.1), and source writes those names. Both parameters of `Array` may be concrete (`Array<Int, 10000>`), forwarded from an enclosing scope (`Array<T, n>`), or inferred by a constructor from a literal (`Array([Int(1), Int(2), Int(3)])`). See [`generics.md`](generics.md) §8.

### 2.7 Intrinsic namespaces

```zane
@primitives$name
@concepts$name
@controlflow$name
@runtime$name
@program$name
```

An **intrinsic** is anything reached through `@`: a type, operation, or instance the compiler supplies rather than a package declares. The `@` namespaces are the **intrinsic namespaces**, and each holds one kind of intrinsic:

- `@primitives$` holds **storage primitives**: the machine-word scalars `@primitives$Int`, `@primitives$Float`, and `@primitives$Bool`, the string view `@primitives$String`, the container primitives of §2.6, opaque runtime primitives used by fundamental types, and the unit type `@primitives$Unit`, whose one value is written `@primitives$Unit()`. An intrinsic that returns nothing, or aborts with nothing, uses `@primitives$Unit`.
- `@concepts$` holds **compiler concept types**, used for source literals and for source constructs that are not storage (§2.8).
- `@controlflow$` holds the **control-flow intrinsics**, the operations that branch, repeat, and exit (§5.1).
- `@runtime$` holds the **runtime types** `@runtime$Console` and `@runtime$Runtime` and their methods ([`effects.md`](effects.md) §6.6).
- `@program$` holds the running program's own instances of those types, `@program$console` and `@program$runtime`.

Each intrinsic operation has exactly one signature, so intrinsics are not overloaded. Methods are the exception: a method's subject is one of its parameters, so intrinsic methods that share a name on different types are overloads, told apart by the subject's type like any other overload ([`functions.md`](functions.md) §4.1).

A namespace is named for what its members are or what they are for. Every member of every intrinsic namespace is an intrinsic, so no namespace takes that word as its name.

Every intrinsic namespace except `@program$` is reachable from every package without an import. `@program$` is reachable only from the root package ([`packages.md`](packages.md) §6.1), which passes its instances to any other package that needs one.

> **Story:** [`stories/syntax.md`](../stories/syntax.md#the-word-every-namespace-shares) — "The word every namespace shares".

### 2.8 Compiler concept types

```zane
@concepts$Int
@concepts$Float
@concepts$String
@concepts$Array<T, n>
@concepts$Map<K, V>
```

These compiler-provided concept types represent source literals before they are lowered into storage types: integer and float literals ([`lexical.md`](lexical.md) §7), string literals, array literals (§2.9), and map literals (§2.10). `Type` in parameter declarations (§2.11) and `@concepts$Block` (§2.12) are concept types too. Concept types may appear in parameter positions but **MUST NOT** be used as storage types such as local variables, fields, or nested storage positions. Functions and constructors may use concept-typed parameters to accept literals and lower them into the corresponding fundamental type.

A concept type that takes no parameters — `Type`, `@concepts$Int`, `@concepts$Float`, and `@concepts$String` — is a **leaf** concept type. A value of a leaf concept type is a compile-time value wherever it appears, including as an argument to a verb. The entries of an array or map literal are ordinary expressions and may be runtime values, so `@concepts$Array<T, n>` and `@concepts$Map<K, V>` carry no such guarantee; a block argument (§2.12) is not a value at all.

A number or string literal becomes storage through the one constructor of the matching storage primitive (see [`types.md`](types.md) §2.7):

```zane
@primitives$Int(value @concepts$Int)
@primitives$Float(value @concepts$Float)
@primitives$String(value @concepts$String)
```

> **Story:** [`stories/types.md`](../stories/types.md#the-literal-that-had-no-way-into-storage) — "The literal that had no way into storage".

### 2.9 Array literals

An **array literal** — a `[ ]` list of values, not an `enum` body or a `match` case group — carries `@concepts$Array<T, n>`, where `T` is the type of its elements and `n` is how many there are. Every element **MUST** already have type `T`; there is no search for a common type across elements that differ. Carrying both parameters is what lets a constructor read an element type and a length off a literal — `Array([Int(1), Int(2), Int(3)])` fixes `T = Int` and `n = 3` (see [`generics.md`](generics.md) §8.1).

A literal with no elements fixes no `T`, so an array literal **MUST** hold at least one element. An empty collection is built by naming its type, which supplies the element type the literal cannot:

```zane
nums Array([Int(1), Int(2), Int(3)]);  // legal: T and n read from the literal
empty Array(Int, 0);                   // legal: the type is named, not inferred
empty [];                              // ILLEGAL: an empty literal fixes no element type
```

### 2.10 Map literals

A **map literal** is a `{ }` body whose entries are `;`-terminated (§6.1 of [`lexical.md`](lexical.md)), each entry exactly two `,`-separated expressions — a key and a value. It carries `@concepts$Map<K, V>`, where `K` is the type of every key and `V` the type of every value. As with an array literal, every key **MUST** already have type `K` and every value type `V`; there is no search for a common type. A map literal **MUST** hold at least one entry, since an empty one fixes neither `K` nor `V`.

```zane
{
    String("first"), Int(1);
    String("second"), Int(2);
}
```

Entries are evaluated in written order. A key is an ordinary expression rather than a name, so two entries may resolve to the same key; the later entry then **replaces** the earlier one. Equality is generally not decidable before run time, so a duplicate is never a compile-time error.

```zane
first String("y");

{
    first, String("hello");    // key is the value of `first`, which is "y"
    String("y"), String("b");  // same key: replaces the entry above
}
```

`@concepts$Map` carries no entry count, unlike `@concepts$Array`. The map literal is not a general-purpose container literal; it is a specialized form for key-value pairs, and `Array` is where generic containment lives. A keyed structure that needs its size in its type is therefore built from an **array of pair values** — an ordinary array literal, which supplies `n` in the ordinary way — rather than from a map literal.

It could not carry a useful count in any case. Entries with equal keys collapse, and a key is an expression, so the number of entries written is only an upper bound on the number stored — where an array literal's `n` is exact.

An empty `{ }` written in a value position with no introducing token is always a code block, never a map literal (§4.8), so the two never compete for the same text. A `{ }` that an introducing token has already claimed — an `init{ }`, a mould body — is governed by that form, not by this rule.

A map literal is one of the two `{ }` arguments that may **trail** a call, the other being a block (§4.8). A literal large enough to want the position gets it for the same reason a block does.

The examples above show the literal alone, with no consumer, because this section fixes the **literal** and the concept type it carries and nothing else; the dynamic container types that consume such a literal — their operations, any ordering, and what they require of a key type — remain unspecified (see [`generics.md`](generics.md) §9).

### 2.11 Parameter concept types

The concept type `Type` declares a type parameter, and `@concepts$Int` — the concept type of an integer literal (§2.8) — declares a number parameter (see [`generics.md`](generics.md) §3). They follow the rule of §2.8: legal in parameter positions, never as storage. A `Type` parameter accepts a type; an `@concepts$Int` parameter accepts a compile-time integer.

### 2.12 The block-argument type

`@concepts$Block` is the type of a **block argument** — a braced run of statements written at a call site and executed by the callee (§4.8). `Block<T>` yields a `T`; a bare `Block` yields nothing. It follows the rule of §2.8 and may never be stored.

```zane
@concepts$Block
@concepts$Block<Bool>
```

> **See also:** [`control-flow.md`](control-flow.md) §2 for what a block argument does.

### 2.13 Function types

A function type leads with its return type, then lists parameter types inside `[ ]`, then any trailing `mut`. There is no `->` arrow. This mirrors the order of function declarations (§3.1–§3.2) and lambda literals (§3.8): the return contract is written first.

```zane
ReturnType[ParamType, ...]
ReturnType?AbortType[ParamType, ...]
ReturnType[this SubjectType, ParamType, ...]
ReturnType[this SubjectType, ParamType, ...] mut
&ReturnType[this SubjectType, ParamType, ...]
ReturnType?AbortType[this SubjectType, ParamType, ...]
ReturnType?AbortType[this SubjectType, ParamType, ...] mut
```

The abort type stays attached to the return type, exactly as in a declaration's `ReturnType?AbortType name(...)` header.

Reference-typed parameters and returns use the ordinary type form. A parameter slot accepts both passing modes — `ParamType` and `&ParamType` — and a return slot accepts a bare or `&` type (§2.3):

```zane
ReturnType[&ParamType, ...]
&ReturnType[this SubjectType, &ParamType, ...]
ReturnType[this SubjectType, ParamType, ...] mut
```

`mut` is legal only when the first parameter is `this`.

```zane
Int[Node, Int] mut    // ILLEGAL: mut requires this as first parameter
Unit[Int, this Node]  // ILLEGAL: this must be the first parameter
```

> **Story:** [`stories/syntax.md`](../stories/syntax.md#two-orders-and-the-one-we-had-already-turned-down) — "Two orders, and the one we had already turned down".

### 2.14 The `#` reference modifier

A leading `#` marks a **reference type**. It attaches only to a **mould** — `#struct { ... }`, `#variant { ... }`, or `#enum [ ... ]` — and only as the right-hand side of a `type`/`alias` declaration (§1.6). The unmarked moulds declare value types.

```zane
type Cell = #struct { value Int; }               // reference product type, declared and named
type Tree = #variant { leaf Int; node Tree; }    // reference sum type; `node` recurses
```

`node` is written as an ordinary hosting member. The compiler boxes such a member because no finite inline layout exists for it — nothing is written for that, and it is not an `&` (see [`adt.md`](adt.md) §4). A value type may recurse the same way; its boxed member is owned by the value and deep-copied with it (see [`memory.md`](memory.md) §2.3).

`&` combines with a reference type and never with a bare value type: an `&T` requires `T` to be a reference type — a declared `#struct`/`#variant`/`#enum` — so a stored **guest** is written `&Cell` or `&Tree` (see [`memory.md`](memory.md) §2.4). See [`types.md`](types.md) §2.1 for the semantics.

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
ReturnType name(param Container<T Type, n @concepts$Int>, ...) { body }
```

A **reference-type** parameter independently selects one of the two passing modes (see [`memory.md`](memory.md) §2.9): bare `ParamType` swallows, `&ParamType` takes a guest. A **value-type** parameter has no such choice — it is always a read-only borrow — so `&` is not written on one.

A function, method, or constructor has no `<>` parameter header. It introduces a type or number parameter inline within its value parameters, at the parameter's first **marked** occurrence — on a value parameter's type (`param T Type`) or inside a value parameter's nested type (`param Container<T Type, n @concepts$Int>`) — and references it bare elsewhere, including in positions written earlier such as the return type. Inline parameters are inferred from the value arguments at the call; the same `Type` / `@concepts$Int` concepts are used as in a type definition's header (§2.5). See [`generics.md`](generics.md) §3 and §5.

> **Story:** [`stories/syntax.md`](../stories/syntax.md#two-orders-and-the-one-we-had-already-turned-down) — "Two orders, and the one we had already turned down".

### 3.2 Methods

```zane
ReturnType name(this SubjectType, param ParamType, ...) { body }
ReturnType name(this SubjectType, param &ParamType, ...) { body }
ReturnType name(this SubjectType, param ParamType, ...) mut { body }
ReturnType name(this SubjectType, param &ParamType, ...) mut { body }
ReturnType?AbortType name(this SubjectType, param ParamType, ...) { body }
ReturnType?AbortType name(this SubjectType, param ParamType, ...) mut { body }
ReturnType name(this SubjectType, param ParamType, ...) => expr
ReturnType name(this SubjectType, param &ParamType, ...) => expr
ReturnType name(this SubjectType, param ParamType, ...) mut => expr
ReturnType name(this SubjectType, param &ParamType, ...) mut => expr
ReturnType?AbortType name(this SubjectType, param ParamType, ...) => expr
ReturnType?AbortType name(this SubjectType, param ParamType, ...) mut => expr
ReturnType name(this SubjectType<T Type, n @concepts$Int>, param ParamType, ...) { body }
```

`this` is legal only in the first parameter position. A declaration is a method if and only if its first parameter is named `this`.

The subject takes **no** marker, for either kind of type: `&` is never written on `this`. A reference-type subject is an implicit guest, which may be stored or returned as `&T` without asking; a value subject is a borrow of the caller's slot, mutable when the method is `mut`. See [`functions.md`](functions.md) §2.4.

`=> expr` returns `expr`, including when `expr` has type `Unit`.

### 3.3 Positional constructors

```zane
TypeName(param ParamType, ...) {
    return init{ field = expr; ... }
}
TypeName(param &ParamType, ...) {
    return init{ field = expr; ... }
}
TypeName(param ParamType, ...) => init{ field = expr; ... }
TypeName(param &ParamType, ...) => init{ field = expr; ... }
TypeName<T>(param T Type, ...) { return init{ field = expr; ... } }
TypeName<T, n>(param Container<T Type, n @concepts$Int>, ...) { return init{ field = expr; ... } }
```

Constructors use the same package-scope declaration shapes as other functions, except that the written type name is the return type and the body constructs the value with `init{ ... }`.

A constructor for a parameterized type has no `<>` header; its name carries the **applied** return type (`TypeName<T>`, `TypeName<T, n>`), whose `<...>` holds bare references to the parameters. It introduces those type and number parameters inline within its value parameters — directly (`param T Type`) or inside a parameter's nested type (`param Container<T Type, n @concepts$Int>`) — in which case they are inferred from the value arguments; or it accepts a type or compile-time integer as an ordinary value parameter of concept type `Type` or `@concepts$Int` (passed explicitly). A constructor is always called by its bare name and **MUST NOT** carry a `<>` list at the call. See [`types.md`](types.md) §3.10 and [`generics.md`](generics.md) §5.

A constructor may carry a **name** — a `.name` suffix on the type — in either the positional or the field form (§3.4), giving a type several named construction paths (see [`types.md`](types.md) §3.4). It is declared and called by that qualified name and yields the base type:

```zane
TypeName.zeros() => init{ field = expr; ... }
TypeName.fromParts(param ParamType, ...) { return init{ field = expr; ... } }
```

```zane
o TypeName.zeros()
p TypeName.fromParts(arg)
```

### 3.4 Field constructors

```zane
TypeName{
    fieldA FieldType;
    fieldB FieldType(args...);
    fieldC FieldType = expr;
    ...
} {
    return init{fieldA; fieldB; fieldC;}
}
TypeName{
    fieldA FieldType;
    fieldB FieldType(args...);
    fieldC FieldType = expr;
    ...
} => init{fieldA; fieldB; fieldC;}
```

A field-constructor header is a `{ }` body, so its entries are `;`-terminated and always trailing ([`lexical.md`](lexical.md) §6.1). Each field entry uses either the bare required-field form `field FieldType` or an initialized storage form such as `field FieldType = expr`. The bare form declares a required constructor input that the call site must supply, not a standalone symbol declaration with its own storage.

Field-constructor call sites may use explicit or implicit field names:

```zane
name TypeName{fieldA = expr; fieldB = expr;}
name TypeName{fieldA; fieldB;}
```

A field-constructor call may omit any field whose constructor entry includes an initializer.

> **Story:** [`stories/syntax.md`](../stories/syntax.md#one-shape-and-everywhere-it-turned-up) — "One shape, and everywhere it turned up".

### 3.5 Implicit constructors

```zane
implicit TypeName(param ParamType) {
    return init{ field = expr; ... }
}
implicit TypeName(param ParamType) => init{ field = expr; ... }
```

Implicit constructors use the `implicit` modifier and are written only in positional form with exactly one parameter.

Illegal forms:

```zane
implicit TypeName() { ... }           // ILLEGAL: exactly one parameter is required
implicit TypeName(a A, b B) { ... }   // ILLEGAL: implicit constructors are single-parameter only
implicit TypeName{field FieldType;} { ... } // ILLEGAL: field-constructor form is not allowed
```

### 3.6 Subscript definitions

```zane
(this SubjectType)[param ParamType, ...] => placeExpr
```

Subscript definitions have no explicit return type annotation. The body **MUST** be a place expression. If the body is not a place expression, the declaration is a compile-time error.

A subscript definition may declare any number of comma-separated parameters inside `[]`. The surface form is not limited to one or two parameters.

The following forms are not part of the grammar:

```zane
ReturnType (this SubjectType)[index ParamType] => expr
```

`[]` is not a general function call form. A subscript definition always declares a place projection that references existing storage within the subject.

> **Story:** [`stories/syntax.md`](../stories/syntax.md#two-orders-and-the-one-we-had-already-turned-down) — "Two orders, and the one we had already turned down".

### 3.7 `init{ }`

```zane
init{
    field = expr;
    otherField;
    ...
}
```

`init{ }` is a `{ }` body, so its fields are `;`-terminated and always trailing ([`lexical.md`](lexical.md) §6.1). A bare field name inside `init{ }` is shorthand for `fieldName = fieldName`.

### 3.8 Lambda literals and lambda-variable declarations

A lambda literal is a function declaration with the name removed. It writes its own return type, parameter types, abort type, and `mut`, exactly like a named function:

```zane
ReturnType() { body }
ReturnType(param ParamType, ...) { body }
ReturnType(param &ParamType, ...) { body }
ReturnType() => expr
ReturnType(param ParamType, ...) => expr
ReturnType?AbortType(param ParamType, ...) { body }
ReturnType(this SubjectType) { body }
ReturnType(this SubjectType) mut { body }
ReturnType(this SubjectType, param ParamType, ...) { body }
ReturnType(this SubjectType, param ParamType, ...) mut { body }
ReturnType(this SubjectType, param ParamType, ...) => expr
ReturnType(this SubjectType, param ParamType, ...) mut => expr
```

A lambda literal omits only the function name. `this` is legal only in the first parameter position. `mut` is legal only when the first parameter is `this`. Parameters and the subject carry the same passing modes as a named verb (§3.1–§3.2).

Examples:

```zane
element!onClick(Unit(eventData EventData) {
    ...
    return Unit();
});

element!onClick(Unit(this Element, data EventData) mut {
    ...
    return Unit();
});
```

A lambda-variable declaration binds a lambda literal to a symbol. The shorthand writes the symbol name in front of the lambda literal and drops the separate `= literal`, mirroring the constructor-call instantiation form `name VarType(args, ...)`:

```zane
name ReturnType(param ParamType, ...) { body }
name ReturnType(param ParamType, ...) => expr
name ReturnType?AbortType(param ParamType, ...) { body }
name ReturnType(this SubjectType, param ParamType, ...) mut { body }
```

The shorthand expands to a symbol declaration whose type is the function type (§2.13) and whose value is the lambda literal:

```zane
callback Unit[this Player] mut = Unit(this Player) mut {
    this.shooting = Bool(false);
    return Unit();
}

callback Unit(this Player) mut {        // shorthand for the line above
    this.shooting = Bool(false);
    return Unit();
}
```

> **Story:** [`stories/syntax.md`](../stories/syntax.md#two-orders-and-the-one-we-had-already-turned-down) — "Two orders, and the one we had already turned down".

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

### 3.10 Return statements

```zane
return expr
```

See [`functions.md`](functions.md) §3.5 for return-path requirements.

---

## 4. Calls and Function Values

### 4.1 Function calls

```zane
name(args...)
packageName$name(args...)
```

### 4.2 Method calls

```zane
subject:method(args...)
subject!method(args...)
subject:packageName$method(args...)
subject!packageName$method(args...)
```

### 4.3 Callables are call-only

Methods, functions, and operators have no value form. A package-scope callable name may appear only in call position; it cannot be written as a bare value.

```zane
packageName$functionName(args...)   // legal: call position
packageName$functionName            // ILLEGAL: callables cannot be referenced as values
+;                                  // ILLEGAL: operators cannot be referenced as values
```

To obtain a function value, declare a lambda-variable (§3.8). A lambda-variable is an ordinary symbol with a single function type, so it carries no overload set.

### 4.4 `spawn`

```zane
spawn functionName(args...)
spawn subject:methodName(args...)
spawn subject!methodName(args...)
spawn functionName(args...) ? binder { ... }
spawn subject:methodName(args...) ? binder { ... }
spawn subject!methodName(args...) ?? fallbackExpr
name VarType = spawn functionName(args...)
name VarType = spawn subject:methodName(args...) ? binder { ... }
name VarType = spawn subject!methodName(args...) ? binder { ... }
name VarType = spawn functionName(args...) ?? fallbackExpr
```

`spawn` is legal only on function-call and method-call expressions. Package-qualified function and method calls use their ordinary forms (§4.1–§4.2). An abortable call may carry a `?` or `??` handler, whether its result is bound or ignored.

### 4.5 Subscript expressions

```zane
placeExpr[argExpr, ...]
```

`[]` is legal only when the subject type defines a subscript declaration. A subscript expression is a place projection, not a general function call, so it is legal only when its base is a place expression.

Examples:

```zane
list[i];
matrix[row, col];
tensor[x, y, z];
```

`CustomList()[1]` is not a valid place expression because the base is a temporary.

### 4.6 Parenthesized expressions

```zane
(expr)
```

Parentheses group an inner expression explicitly. See [`operators.md`](operators.md) §3 for precedence.

Example:

```zane
number Int = (3 + 2) * 2;
```

### 4.7 `match` expressions

A `match` expression names one or more scrutinees — a bare `,`-separated list, never parenthesised — then a `{ }` block of `;`-terminated arms. Each arm is an optional binder, a case selector, `=>`, and a body. A `match` may appear anywhere an expression is legal and may carry a trailing `?` (or `??`) handler when its arms are abortable.

```zane
match scrutinee { arm; arm; ... }
match scrutinee, scrutinee { arm; arm; ... }
name VarType = match scrutinee { arm; arm; ... }
name VarType = match scrutinee { arm; arm; ... } ? binder { ... }
return match scrutinee { arm; arm; ... }
functionName(match scrutinee { arm; arm; ... })
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

### 4.8 Block arguments and trailing arguments

A call may carry any number of **block arguments**, one for each `@concepts$Block` parameter the callee declares. Each is an ordinary argument written in argument position.

```zane
repeatTwice({ console!print("hi"); });
```

A call's **last** argument may instead **trail**: it is written after the closing `)` rather than inside it, and the `)` is elided. Only a `{ }` argument may trail — a block or a map literal (§2.10). An array literal is always written inside the parentheses. At most one argument trails per call.

```zane
repeatTwice() {
    console!print("hi");
}

ran Bool = if(ready) {
    start();
}
```

A trailing argument **MUST** be the last thing in its statement: the `}` that closes it ends the statement, so neither a `;` nor anything that would continue the call may come after it (§6.3 of [`lexical.md`](lexical.md)). The brace ends the call and the statement together, which is what the elided `)` would otherwise have to do in two marks.

A call that supplies more than one block writes the earlier ones as ordinary arguments and may still trail the last:

```zane
ran!elif({ expensive(); }) {
    handle();
}
```

The trailing and parenthesized forms are the same call. The `)` moves to where the statement ends:

```zane
if(true) {
    console!print("hi");
}

if(true, { console!print("hi"); });   // the same call, written in full
```

A block takes no parameters and is never named. A block that yields a value ends its yielding paths with `resolve` (§6.2 uses the same keyword at a handler):

```zane
value Int = compute() {
    resolve Int(3);
}
```

```zane
f({ x; }, { y; });    // legal: two block arguments, neither trailing
f({ x; }) { y; }      // legal: the same call with the last one trailing
f({ x; }) { y; } ()   // ILLEGAL: the `}` already ended the statement
g();
{
    console!print("oops");    // ILLEGAL: a `{ }` may not open a statement (§6.3.1 of lexical.md)
}
```

> **Story:** [`stories/lexical.md`](../stories/lexical.md#what-had-to-be-true-before-a-brace-could-end-a-statement) — "What had to be true before a brace could end a statement".
> **Story:** [`stories/syntax.md`](../stories/syntax.md#the-list-that-stayed-inside-the-parentheses) — "The list that stayed inside the parentheses" tells why a `[ ]` does not trail.

An argument list with nothing left inside it still writes its `( )`; the trailing form elides only the `)`, never the whole list:

```zane
do() {
    tmp Int(9);
    use(tmp);
}
```

---

## 5. Control Flow

Zane has no control-flow grammar. Branching, repetition, and exiting are all calls, declared by the `core` package (see [`control-flow.md`](control-flow.md) §3); the first two are ordinary calls with block arguments (§4.8), and the exit is an ordinary call over the intrinsic below.

### 5.1 Control-flow intrinsics

```zane
@controlflow$branch(condition @primitives$Bool, body @concepts$Block)
@controlflow$repeat(count @primitives$Int, body @concepts$Block)
@controlflow$exitFromCall()
```

The first two take storage primitives rather than fundamental types and the third takes nothing, so none depends on any package. Any package may call them.

### 5.2 Writing an exit

`@controlflow$exitFromCall()` ends the invocation that called the verb whose body contains it, so an exit is written by *calling* a verb built on it — `core` supplies `guard`:

```zane
guard(shouldStop);
```

The intrinsic itself appears in the body of such a verb, not at the point an exit is wanted; written in a verb's own body it would end that verb's caller ([`control-flow.md`](control-flow.md) §4.2).

---

## 6. Error Handling

### 6.1 Abortable return types

```zane
ReturnType?AbortType
```

### 6.2 `?` handlers

```zane
expr ? binder { ... }
```

Every path inside the handler must end with one of:

```zane
resolve expr
return expr
abort expr
```

### 6.3 `??` shorthand

```zane
expr ?? fallbackExpr
```

---

## 7. Operators and Keywords

### 7.1 Operators

`~`, `*`, `/`, `+`, `-`, `<`, `>`, `<=`, `>=`, `==`, `~=`

Every binary operator above also has a **loose form**, written with a leading `'`:

`'*`, `'/`, `'+`, `'-`, `'<`, `'>`, `'<=`, `'>=`, `'==`, `'~=`

```zane
a == b '* c == d      // legal
a ''* b               // ILLEGAL: there is no second loose tier
'~a                   // ILLEGAL: unary operators have no loose form
```

> **See also:** [`operators.md`](operators.md) §3.1 for where the loose forms group.

### 7.2 Control-flow keywords

Zane has none.

`if`, `elif`, `else`, and `guard` are not keywords. They are `core` declarations called like any other verb (see [`control-flow.md`](control-flow.md) §3). `guard` in particular is an ordinary verb over the exit intrinsic (§5.2), not grammar. Counted repetition is a method call on the counter rather than a named construct — `i!to(end)` ([`control-flow.md`](control-flow.md) §3.4).

### 7.3 Comments

```zane
// single-line comment
/// doc comment
```

Zane has no block-comment syntax. `//` starts a single-line comment. `///` starts a documentation comment line. Adjacent `///` lines are merged into one documentation block.

> **Story:** [`stories/syntax.md`](../stories/syntax.md#every-line-admits-to-being-a-comment) — "Every line admits to being a comment".

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
