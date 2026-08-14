# Zane Types

This document specifies Zane's data types: fundamental, value, and reference types; the `#` modifier; fields; constructors; `type` and `alias` declarations; and the `init{ }` expression. Methods and other behavior live in [`functions.md`](functions.md).

> **See also:** [`memory.md`](memory.md) §2 for hosting rules. [`functions.md`](functions.md) for methods and functions. [`syntax.md`](syntax.md) §1 and §3 for declaration grammar.

---

## 1. Overview

Zane keeps data layout and construction separate from behavior.

- **`Fields-only type bodies`.** A type body declares storage only — no methods or constructors live inside the body.
- **`One kind axis`.** A type is a **value type** unless its mould is marked `#`, which makes it a **reference type** — identity-bearing, aliasable through `&`, able to hold reference-type and `&` fields, and moved rather than copied. `struct` is a value mould; `#struct` a reference mould. Either kind may recurse (see [`adt.md`](adt.md) §4).
- **`Package-scope constructors`.** A constructor is a verb at package scope; the body builds the value with `init{ }`.
- **`Name-based field privacy`.** A leading `_` makes a field private to methods whose first parameter is `this` for that type.
- **`Fundamental and declared types`.** `Int`, `Float`, `Bool`, `String`, and `Unit` belong to the language; `type` introduces a new distinct named type and `alias` an interchangeable name.

---

## 2. Value and Reference Types

### 2.1 The value/reference axis and the `#` modifier
Every mould is a **value mould** unless it is marked with `#`, which makes it a **reference mould**; these are its **value form** and its **reference form**. A type declared with a value mould is a **value type**; one declared with a reference mould is a **reference type**. This value/reference axis is orthogonal to the *shape* of the mould (such as a product `struct` or a sum `variant`, see §2.5). For the product shape, `struct` is the value mould and `#struct` the reference mould. The `#` mark applies only to a **mould** — `#struct`, `#variant`, or `#enum` (see [`adt.md`](adt.md) §2 and §3 for `#enum` and `#variant`) — and only where a type is declared (§5.3). A reference type is a **distinct type** from any value type; it reuses only the field layout of its mould and otherwise has its own identity, its own constructors, and its own methods (see [`memory.md`](memory.md) §2).

A **value type** is copied on assignment, has no identity, and is *transitively* a value: it may contain only other value types, never a reference-type or `&` field (§2.2, [`memory.md`](memory.md) §2.10). A **reference type** has single hosting and stable identity, follows the rules in [`memory.md`](memory.md) §2, may be aliased through `&`, may hold reference-type and `&` fields, and is moved rather than copied. Either kind may **recurse**, through a member the compiler boxes (see [`adt.md`](adt.md) §4). Placement — stack or heap — is an unobservable implementation choice for both kinds (see [`memory.md`](memory.md) §3.5).

```zane
package Graph

type Node = #struct {      // reference type: identity, may hold `&`, moved not copied
    _id Int;
    scale Float;
    label String;
}
```

> **Story:** [`stories/types.md`](../stories/types.md#two-axes-instead-of-a-keyword-per-kind) — "Two axes instead of a keyword per kind".
> **Story:** [`stories/types.md`](../stories/types.md#what--actually-changes-and-the-boxing-trap) — "What `#` actually changes, and the boxing trap".

### 2.2 Value types are transitive and mutable in place
A value-type body contains only field declarations, stored inline apart from any member the compiler boxes (see [`memory.md`](memory.md) §3.3). A value type **MUST NOT** contain a reference-type or `&` field, and this holds transitively: a value type reachable through a value type must itself be a value type (see [`memory.md`](memory.md) §2.10). The restriction is what makes a value copyable and shareable-by-snapshot with no hosting or anchor bookkeeping. It does not stop a value type from containing *itself* (see [`adt.md`](adt.md) §4).

A value is **mutable in place**: a `mut` method may write its fields, because the subject is a *borrow* of the caller's storage rather than a copy (see [`effects.md`](effects.md) §2.3 and [`functions.md`](functions.md) §2.4). A value's storage slot may also be overwritten wholesale.

```zane
package Math

type Vec2 = struct {       // value type: copied, transitively value, mutable in place
    x Float;
    y Float;
}

pos Vec2(1, 2)
pos!setX(Float(3))   // legal: mut method writes the field through a borrow of pos
pos = Vec2(3, 4)     // legal: overwrites the whole value
```

### 2.3 Field visibility is name-based
Fields whose names begin with `_` are private to methods whose first parameter is `this` for that type, regardless of which package declares the method.

The same subject type written under any other parameter name is a non-subject parameter and does not gain private-field access.

All fields whose names do not begin with `_` are public.

This is intentional: private-field access in Zane is method-based, not package-based.

> **Story:** [`stories/lexical.md`](../stories/lexical.md#privacy-lives-in-the-name) — "Privacy lives in the name".

### 2.4 Type bodies contain no behavior
Methods, constructors, overload rules, and function values live at package scope. A reader can inspect a type body to learn layout without scanning for behavior.

### 2.5 `struct` and `variant` share one body grammar
A `struct` is a **product mould**: a value of the type it declares has all of its members at once. A `variant` is a **sum mould** with the same body grammar: a value has exactly one of its members at a time. The body of a `variant` is byte-for-byte the same shape as a `struct` body; the keyword alone flips product into sum.

```zane
type Color = struct { r Int; g Int; b Int; }    // value product type: has r and g and b
type Shape = variant { dot Dot; line Line; }      // value sum type: has dot or line
```

The `#` modifier (§2.1) is the other axis: `struct`/`#struct` are the product pair, `variant`/`#variant` the sum pair. A value mould — `struct` or `variant` — declares a value type: transitively value, so it **MUST NOT** contain a reference-type field or an `&` field (§2.2, [`memory.md`](memory.md) §2.10). A reference mould — `#struct` or `#variant` — declares a reference type, which may hold reference-type and `&` fields. Both may recurse, their recursive members boxed into the dynamic region (see [`adt.md`](adt.md) §4). The body syntax is symmetric across these four combinations; the keyword picks product versus sum and the `#` picks value versus reference. Because `#` marks only a mould, a reference type comes into being only through such a declaration and is always named there (§5.3).

> **Story:** [`stories/types.md`](../stories/types.md#confining--to-the-body-forms) — "Confining `#` to the body forms".

> **See also:** [`adt.md`](adt.md) for the canonical rules on `variant`, `enum`, pattern matching, and enum maps. [`adt.md`](adt.md) §3 for the full struct-versus-variant symmetry.

### 2.6 Fundamental language types

`Int`, `Float`, `Bool`, `String`, and `Unit` are **fundamental language types**. Their names are available unqualified in every source file. The compiler distribution supplies their declarations through a bundled `core` implementation package. That package is not part of the source package system: programs do not import it, qualify its members, list it as a dependency, or replace it independently of the compiler version.

Control-flow constructs refer to these semantic types rather than to their storage primitives. Conditions expect `Bool`, counted-loop bounds expect `Int`, and the loop variable has type `Int`; see [`control-flow.md`](control-flow.md) §2–§4. Programs never unwrap a fundamental type to feed a primitive into control flow.

The bundled `core` package defines the constructors and methods of fundamental types over storage primitives in the `@primitives$` namespace. Compiler concept types represent source literals until coercion selects one of those constructors: `true` becomes `Bool` in a condition and `20` becomes `Int` at a counted-loop bound. Explicit `Bool(true)` and `Int(20)` construction remains legal. This does not create general truthiness or numeric narrowing.

`Unit` is the unit type. Its bundled declaration is an empty value `struct`, so it has exactly one logical value and zero-sized storage. `Unit()` is its ordinary `core` constructor. It may appear wherever any other value type may appear, including symbols, fields, arrays, generic arguments, function parameters, and return types.

```zane
type Player<T Type> = #struct {
    name String;
    extraSettings T;
}

Player<T>(name String, extraSettings T Type) => init{name, extraSettings}

player Player<Unit> = Player("Manuel", Unit())
completed Unit = performWork()
```

An implementation may erase only the runtime storage of `Unit` values, including fields, array elements, and constructor results. It **MUST** still evaluate every expression that produces a `Unit` value at its original program point and in its original order. Storage erasure never removes side effects or otherwise changes observable evaluation.

> **Story:** [`stories/types.md`](../stories/types.md#unit-exposes-the-package-that-wasnt-one) — "Unit exposes the package that wasn't one".

---

## 3. Constructors and Initialization

### 3.1 Constructors are package-scope declarations
A constructor is a package-scope verb named after the type. It has no `this` parameter because no object exists yet.

Constructors use the same block-bodied or expression-bodied surface forms as other package-scope verbs, except that the written type name is the return type and the body constructs the result with `init{ ... }`.

> **Story:** [`stories/types.md`](../stories/types.md#naming-a-type-by-what-it-is-not-how-it-is-built) — "Naming a type by what it is, not how it is built".

### 3.2 Positional constructors
Positional constructors declare ordinary parameters and return `init{ ... }`.

```zane
package Graph

Node(id Int, scale Float, label String) {
    return init{
        _id = id,
        scale = scale,
        label = label
    }
}
```

Expression-bodied constructors are equivalent. As for any verb, `=> expr` is pure shorthand for `{ return expr }`, so the form below means exactly `{ return init{x, y} }`:

```zane
package Math

Vec2(x Float, y Float) => init{x, y}
```

Positional constructors **MAY** be overloaded by arity or parameter types.

Constructor parameters do not need to repeat private-field underscores. It is normal to map a public-facing parameter such as `id` into a private field such as `_id` explicitly inside `init{ }`.

A constructor body is an ordinary verb body: a sequence of statements that ends by returning an `init{ }`. It may run any computation before that return, exactly like a function. A constructor is nothing more than a verb that is named after its type and returns `init{ }` — it is indistinguishable from a builder helper that ends in a constructor call, apart from that sugar.

```zane
package Graph

Node(id Int, scale Float, label String) {
    scaled Float = scale * scale       // ordinary statements run first
    nextId Int = id + Int(1)
    return init{
        _id = nextId,
        scale = scaled,
        label = label
    }
}
```

### 3.3 Field constructors
A constructor may also declare fields directly in its parameter header:

```zane
package Math

type Vector = struct {
    x Int;
    y Int;
}

Vector{x Int, y Int} {
    return init{x, y}
}
```

This form is the canonical constructor syntax when the constructor parameters map directly to fields.

Field-constructor entries may also declare default values. They use the same initialized declaration forms as ordinary storage declarations. A call may omit any field whose constructor entry provides one:

```zane
type Weapon = #struct {
    name String;
    fireRate Float;
    damage Float;
}

Weapon{
    name String = "Pistol",
    fireRate Float(1),
    damage Float = 10
} {
    return init{name, fireRate, damage}
}

starter Weapon{fireRate = Float(2)}
```

### 3.4 Named constructors
The by-type-name constructor (§3.2, §3.3) is the **anonymous** one. A type may also declare **named** constructors, each a verb whose name is the type followed by a `.name` suffix, giving one type several construction paths a bare `Type(...)` cannot tell apart.

```zane
package Math

type Vector2 = struct { x Float; y Float; }

Vector2.zeros() => init{ x = Float(0), y = Float(0) }
Vector2.diagonal(n Float) => init{ x = n, y = n }

o Vector2.zeros()            // o : Vector2
d Vector2.diagonal(Float(3)) // d : Vector2
```

A named constructor is an ordinary constructor in every other respect. Naming a verb after a type — with or without the `.name` suffix — is the capability marker that makes it a constructor (see [`functions.md`](functions.md) §8.2): the return type is implicit (the type named, `Vector2`) and `init{ }` is unlocked. The suffix only distinguishes it; it does not change what it returns. So a named constructor:

- declares positional parameters or the field-header form (§3.2, §3.3) like any constructor;
- overloads by parameter types, alongside the anonymous constructor and the other named ones;
- is called by its qualified name and yields the **base type** — `Vector2.zeros()` is a `Vector2`, never a `Vector2.zeros` type.

The casing rule (see [`lexical.md`](lexical.md) §3) keeps the call unambiguous: `Vector2.zeros()` has an uppercase subject, so `.zeros` is a member of the *type* — a constructor — while `v.zeros` has a lowercase subject, so `.zeros` is a field or method of a *value*. The two never collide.

A named constructor **MUST NOT** be marked `implicit`: an implicit constructor is an anonymous single-argument conversion the compiler inserts at a coercion site (§4), and a name has nothing to insert.

Because a named constructor builds through `init{ }`, it belongs to a type that has fields — a `struct`, in either its value or `#` reference form (§3). A `variant` has cases, not fields, and is built by naming a case (see [`adt.md`](adt.md) §3.2), which is built-in syntax rather than a constructor verb; the two share the `Type.member(args)` surface but not the mechanism.

> **Story:** [`stories/types.md`](../stories/types.md#named-constructors-and-the-syntax-variants-already-had) — "Named constructors, and the syntax variants already had".

### 3.5 Implicit field access in constructor calls
Field-constructor call sites may use implicit field access when the argument expression name matches the field name:

```zane
x Int(3)
y Int(2)
vec Vector{x, y}
```

`Vector{x, y}` is shorthand for `Vector{x = x, y = y}`.

### 3.6 Implicit field access in `init{ }`
Inside `init{ }`, a bare field name is shorthand for `fieldName = fieldName` when a symbol of that name is in scope:

```zane
Vector{x Int, y Int} {
    return init{x, y}
}
```

This is shorthand for:

```zane
Vector{x Int, y Int} {
    return init{
        x = x,
        y = y
    }
}
```

### 3.7 `init{ }` is a constructor-only expression
`init{ }` is valid only inside a constructor body, but within that body it is an ordinary expression of the enclosing constructor's type. It may be returned directly or assigned to a local before being returned.

```zane
Vector{x Int, y Int} {
    temp Vector = init{
        x = x,
        y = y
    }
    return temp
}
```

Every field of the target type **MUST** be assigned exactly once, either explicitly or through implicit field access shorthand.

### 3.8 Constructors do not use `mut`
Constructors are not methods. They create new values rather than mutating an existing subject, so `mut` does not apply.

### 3.9 `&` fields require `&` constructor parameters
An `&` field is legal only in a reference type (`#struct`/`#variant`), since a value type is transitively value (§2.2). A constructor that assigns a value to an `&` field must declare the corresponding parameter as `&T` — a swallowing `T` will not do, because the swallowed value is hosted at the call site while the field outlives it ([`memory.md`](memory.md) §2.9). The caller must then supply a **guest source** under [`memory.md`](memory.md) §2.8: a bare symbol, a field access on a place, or an `&T` parameter. A temporary and a `[]` expression are rejected.

```zane
package Vehicle

type Car = #struct {
    engine &Engine;
}

// legal: `&` parameter allows storing into `&` field
Car(engine &Engine) {
    return init{engine = engine}
}
```

```zane
// ILLEGAL: plain parameter cannot be bound into `&` storage
Car(engine Engine) {
    return init{engine = engine}   // ERROR: plain parameter MUST NOT be bound into `&` storage
}
```

Call sites:

```zane
garage Garage()
car Car(garage.spare)   // legal: a field access is a guest source
```

```zane
engine Engine()
car Car(engine)     // legal: a bare symbol is a guest source
car Car(Engine())   // ILLEGAL: a temporary cannot initialize an `&` field
```

What still constrains such a field is scope, not source: the object it points at must be hosted in the same or a higher lexical scope than the `&` itself ([`lifetimes.md`](lifetimes.md) §1.1). Because a field's scope is its container's, two further rules carry that comparison past construction: an assignment into an `&` field or element must share a root with its source ([`lifetimes.md`](lifetimes.md) §1.10), and raising a value that holds an `&` re-checks what that `&` names ([`lifetimes.md`](lifetimes.md) §1.11). Recursion is not one of these cases at all: a recursive member is an ordinary owning field the compiler boxes, so it needs no `&` and no guest source (see [`adt.md`](adt.md) §4).

> **Story:** [`stories/memory.md`](../stories/memory.md#the-ban-that-cost-more-than-the-question-it-closed) — "The ban that cost more than the question it closed".

A reference type whose fields are all plain hosts does not require `&` parameters:

```zane
type Car = #struct {
    engine Engine;
}

Car(engine Engine) {
    return init{engine = engine}
}

car Car(Engine())   // legal: plain host field accepts a temporary
```

### 3.10 Type and number parameters
A constructor for a parameterized type receives its type and number parameters in one of two ways, because a constructor call never carries a `<>` type-argument list. A constructor has no `<>` header: a parameter introduced inline — on a value parameter's type or in a nested type — is inferred from the value arguments; a parameter declared as a `Type` or `Number` value parameter is passed explicitly as an ordinary argument.

```zane
// inferred: T is introduced inline and deduced from the value arguments
Vector<T>(x T Type, y T Type) {
    return init{x, y}
}

// explicit: the type and size are passed as arguments
Array<T, n>(T Type, n Number) {
    // zero-initialise n elements of type T
}
```

The constructor's name is its return type, so a constructor for a parameterized type names the applied type (`Vector<T>`, `Array<T, n>`); the `<...>` holds bare references to the inline-introduced or explicitly passed parameters (it carries `T`, not `T Type`, so it is not a reintroduced header). The call is always by bare name.

A `Type` value parameter is usable as a type inside the body (for example, `T(0)`); a `Number` value parameter is usable as a number. A constructor is always called by its bare name: `Vector(Int(2), Int(3))` infers `T`, while `Array(Int, 10000)` passes the type and size explicitly.

> **See also:** [`generics.md`](generics.md) §5 for the complete rules on how types and numbers reach a constructor, and §3 for the unified parameter system.

---

## 4. Implicit Constructors

### 4.1 The `implicit` modifier
A constructor marked with the `implicit` modifier declares a single-parameter conversion. Implicit constructors **MUST** declare exactly one parameter and **MUST NOT** use field-constructor form.

```zane
package Units

type Meters = struct {
    value Float;
}

type Feet = struct {
    value Float;
}

// implicit conversion from Feet to Meters
implicit Meters(feet Feet) => init{value = feet.value * Float(0.3048)}

Unit printDistance(d Meters) {
    ...
    return Unit()
}
```

At a **coercion site** — a position whose destination type is fixed by a callable or language construct (see §4.2) — if the source expression has a different type and exactly one applicable implicit constructor exists, the compiler inserts that constructor call automatically.

```zane
printDistance(Feet(Float(10)))   // coercion site: parameter expects Meters, Feet provided
// desugars to: printDistance(Meters(Feet(Float(10))))
```

A named field entry of a field-constructor call is a coercion site too, so the conversion fires inside the braces:

```zane
type Trip = struct { distance Meters; label String; }
Trip{distance Meters, label String} => init{distance, label}

Trip{distance = Feet(Float(10)), label = "hike"}   // field entry expects Meters, Feet provided
// desugars to: Trip{distance = Meters(Feet(Float(10))), label = "hike"}
```

The `init{ }` inside a constructor body is **not** a coercion site: there the constructor writes its own value's fields, so like a `return` the conversion is written explicitly (see §4.2).

A declaration is **not** a coercion site, so the conversion must be written explicitly there:

```zane
distance Meters = Feet(Float(10))           // ILLEGAL: a declaration is not a coercion site
distance Meters = Meters(Feet(Float(10)))   // legal: explicit conversion
```

### 4.2 Coercion sites
A coercion site is a position that passes a value into a contract whose destination type is fixed by a callable or language construct. These are the only positions where the compiler inserts an implicit constructor:

- Positional arguments of a function call
- Positional arguments of a method call (the subject is excluded; see §4.6)
- Positional arguments of a positional constructor call `Type(...)`
- Positional arguments of a named-constructor call `Type.name(...)`
- Named field entries of a field-constructor call `Type{ field = expr }`
- Condition expressions of `if`, `elif`, and `guard`, whose destination type is `Bool`
- The `start` and `end` expressions of a counted `loop`, whose destination type is `Int`

Anonymous and named positional constructors use their declared parameter types identically, so `Type(...)` and `Type.name(...)` arguments receive the same implicit conversions. A field-constructor call entry fills the constructor's declared slot in the same way. A control-flow expression fills a slot fixed by the language instead: `Bool` for a condition and `Int` for a counted-loop bound.

An implicit constructor is **never** inserted at any other position. In particular, the following are **not** coercion sites:

- Symbol declarations with a type annotation: `name VarType = expr`
- Assignments to already-declared symbols: `name = expr`
- Field or subscript assignments: `obj.field = expr`, `arr[i] = expr`
- `return` expressions, even when the return type is declared
- Named field entries of an `init{ field = expr }` initializer inside a constructor body

At each of these positions the destination type is one you fix yourself — a local declaration, existing storage, the return type in the enclosing signature, or the fields the constructor builds through `init{ }` — rather than a contract supplied by a callee or language construct, so the conversion must be written explicitly.

Operator operands **are** coercion sites, because operators desugar to ordinary function calls (see [operators.md](operators.md) §2.2); each operand is a positional argument of that call.

At one coercion site requiring destination type `T`, given an argument with static type `U`, the compiler resolves the site locally:

1. If `U` is exactly `T`, accept the argument with no insertion.
2. Otherwise, collect all visible applicable `implicit` constructors from `U` to `T`.
3. If exactly one applicable implicit constructor exists, rewrite the argument as `T(arg)`.
4. If multiple applicable implicit constructors exist, the site is an ambiguity error.
5. If none exist, the site is a normal type error.

For candidate collection, implicit constructors declared in the bundled `core` package are automatically visible and applicable at every coercion site. They require no source import or qualification. In particular, `core` declares the implicit constructors from compiler concept types to the corresponding fundamental types, so literal coercion follows this same algorithm rather than a separate compiler-only lowering rule.

### 4.3 No chaining
Implicit conversions are never chained. If no single-step implicit constructor exists from source type `U` to destination type `T`, the compiler does not search for a path `U → V → T`. The call is a type error.

### 4.4 Source and destination type constraints
The **source type** (parameter type) of an implicit constructor **MUST** be a value type or a compiler concept type in the `@concepts$` namespace. It **MUST NOT** be a reference type or an `&`.

The **destination type** (return type, i.e., the type name of the constructor) **MAY** be a value type or a reference type.

```zane
type Celsius = struct { value Float; }
type Fahrenheit = struct { value Float; }

implicit Celsius(f Fahrenheit) => init{value = (f.value - Float(32)) * Float(5) / Float(9)}   // legal: value → value
```

```zane
type Logger = #struct { verbosity Int; }
type LogConfig = struct { verbosity Int; }

implicit Logger(cfg LogConfig) {   // legal: value → reference
    return init{verbosity = cfg.verbosity}
}
```

```zane
type Source = #struct { data String; }
type Destination = #struct { payload String; }

implicit Destination(s Source) {   // ILLEGAL: source type is a reference type
    return init{payload = s.data}
}
```

### 4.5 Coherence and the orphan rule
An implicit constructor from type `U` to type `T` **MUST** be declared in the home package of either type. A third-party package **MUST NOT** declare an implicit constructor between two imported types. The bundled `core` implementation is the home package of fundamental types, but source packages cannot add declarations to it; a fundamental endpoint therefore does not by itself grant a source package permission to declare a conversion.

This rule prevents conflicts when multiple packages independently define the same implicit conversion and ensures that the owner of at least one type controls the conversion behavior.

```zane
package Units

type Meters = struct { value Float; }

// legal: declared in home package of Meters
implicit Meters(feet Feet) => init{value = feet.value * Float(0.3048)}
```

```zane
package Conversions
import Units

// ILLEGAL: neither Meters nor Feet is defined in Conversions
implicit Units$Meters(feet Units$Feet) => init{value = feet.value * Float(0.3048)}
```

### 4.6 Method subjects are never implicitly converted
The subject expression (`this`) in a method call is never subject to implicit conversion. This remains true even though method calls desugar to ordinary function calls. If the subject type does not match, the call is a type error.

```zane
Unit logDistance(this Meters) {
    ...
    return Unit()
}

feet Feet(Float(10))
feet:logDistance()   // ILLEGAL: subject type is Feet, not Meters
```

> **See also:** [`functions.md`](functions.md) §5 for how implicit constructors interact with overload resolution.
> **Story:** [`stories/types.md`](../stories/types.md#coercion-follows-the-call-not-the-store) — "Coercion follows the call, not the store".

---

## 5. Type Definitions and Aliases

Zane names types with two declaration keywords. `type` introduces a new distinct type; `alias` introduces an interchangeable name. Both use `=` as the delimiter, so the keyword alone carries the distinction.

### 5.1 `type` declares a distinct type
A `type` declaration introduces a new, distinct named type. The new type is structurally equal to its right-hand side but is **not** interchangeable with it.

```zane
type VectorInt = Vector<Int>   // distinct type; NOT interchangeable with Vector<Int>
```

A value of `VectorInt` and a value of `Vector<Int>` do not substitute for each other implicitly, even though their layouts match.

### 5.2 `alias` declares an interchangeable name
An `alias` declaration introduces a true alias: the new name and its right-hand side are fully interchangeable everywhere.

```zane
alias VectorInt = Vector<Int>   // fully interchangeable with Vector<Int>
```

### 5.3 The right-hand side is a type expression
The right-hand side of a `type` or `alias` declaration is any type expression: an applied generic (`Vector<Int>`), an `Array<Int, 10000>`, or an inline mould — `struct { ... }`, `variant { ... }`, or `enum [ ... ]` — in either its value form or its `#` reference form.

```zane
type Wrapper = struct {
    vec Vector<Int>;
    arr Array<Int, 10000>;
}
```

A named type is therefore always declared this way: `type Name = struct { ... }` or `type Name = #struct { ... }` (and likewise `variant`/`#variant`/`enum`). There is no standalone `struct Name { ... }` declaration form — a mould is a type expression that only names a type through a `type` (or `alias`) declaration.

These three forms — `struct`, `variant`, and `enum` — are the **moulds**: the constructs that give a type its shape. Each has a value form and a `#` reference form (§2.1), and a mould **MUST** appear only as the right-hand side of a `type` or `alias` declaration. Every other type position — a field, a parameter, a return type — names a declared type or an instantiation of one (`Weapon`, `Vector<Int>`, `Array<Int, 10000>`, `&Node`). Every constructible type therefore has a name, and that name is what its constructor is called by (§3.1). Fundamental types follow the same declaration model inside the bundled `core` implementation; see §2.6.

> **Story:** [`stories/types.md`](../stories/types.md#every-type-has-a-name-because-construction-needs-one) — "Every type has a name, because construction needs one".
> **Story:** [`stories/types.md`](../stories/types.md#naming-the-moulds-and-marking-every-one) — "Naming the moulds, and marking every one".

### 5.4 The keyword carries the distinction
Intent lives entirely in the keyword — `type` versus `alias` — not in the punctuation. The `=` delimiter is identical in both forms, which keeps them visually parallel while making the distinct-vs-interchangeable choice explicit.

> **See also:** [`generics.md`](generics.md) §4 for type expressions and [`syntax.md`](syntax.md) §1 for the declaration grammar.

---

## 6. Summary

| Concept | Rule |
|---|---|
| Type body | Fields only — no methods or constructors inside the body |
| Value/reference axis | A type is a value type unless marked `#`; `#` marks only a mould — `#struct`/`#variant`/`#enum` (declared and named), each a distinct reference type with identity and `&`-aliasing, moved rather than copied; the unmarked moulds declare value types, and either kind may recurse |
| Mould | One of the three type-shaping forms — `struct`, `variant`, or `enum`; each has a value form and a `#` reference form; appears only as a `type`/`alias` right-hand side, so every constructible type is named |
| Use-site types | A field, parameter, or return type names a declared type or an instantiation (`Weapon`, `Vector<Int>`, `&Node`); a mould appears only as a `type`/`alias` right-hand side |
| Value type | Copied on assignment; transitively value (no reference-type or `&` field, anywhere downstream); mutable in place through a borrowed `mut` subject; storage may also be overwritten wholesale |
| Reference type (`#`) | Single hosting and stable identity; may hold reference-type and `&` fields; moved rather than copied; placement is unobservable |
| Fundamental type | `Int`, `Float`, `Bool`, `String`, or `Unit`; declared by the bundled `core` implementation and available unqualified |
| `Unit` | Empty `core` value type; `Unit()` constructs its sole value, which may be stored or used as a generic argument |
| Field visibility | Names starting with `_` are private to `this`-parameter methods on the subject type; all other names are public |
| Constructor | Package-scope verb named after the type; the written type name is the return type; no `this`; may use block or `=> init{...}` form |
| Field constructor | Declares field parameters directly, may assign default values, and may use `init{field}` shorthand |
| Implicit constructor | Single-parameter constructor marked `implicit`; inserted at callable arguments, named field-constructor entries, conditions, and counted-loop bounds — never at declarations, assignments, stores, `return`, or the `init{field = value}` inside a constructor body; no field-constructor form; source type must be a value type or compiler concept; orphan rule applies |
| `&` constructor parameter | Caller must supply an allowed `&` source; callee may store into `&` fields |
| Plain `T` constructor parameter | Value-only; caller may supply a temporary; callee **MUST NOT** bind it into `&` storage |
| `Type` / `Number` constructor parameter | Accepts a type or a compile-time number; inferred from inline introduction or passed explicitly as a value parameter |
| `type` declaration | Introduces a new distinct type, structurally equal to its right-hand side but not interchangeable with it |
| `alias` declaration | Introduces an interchangeable alternate name for a type expression |
