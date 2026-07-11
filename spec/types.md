# Zane Types

This document specifies Zane's data types: value and reference types, the `#` modifier, fields, constructors, `type` and `alias` declarations, and the `init{ }` expression. Methods and other behavior live in [`functions.md`](functions.md).

> **See also:** [`memory.md`](memory.md) §2 for ownership rules. [`functions.md`](functions.md) for methods and functions. [`syntax.md`](syntax.md) §1 and §3 for declaration grammar.

---

## 1. Overview

Zane keeps data layout and construction separate from behavior.

- **`Fields-only type bodies`.** A type body declares storage only — no methods or constructors live inside the body.
- **`One kind axis`.** A type is a **value type** unless marked `#`, which makes it a **reference type**. `struct` is the value product; `#struct` is the reference product — identity-bearing, aliasable through `&`, and able to hold reference-type and `&` fields and recurse.
- **`Package-scope constructors`.** A constructor is a verb at package scope; the body builds the value with `init{ }`.
- **`Name-based field privacy`.** A leading `_` makes a field private to methods whose first parameter is `this` for that type.
- **`Named types and aliases`.** `type` introduces a new distinct named type; `alias` introduces an interchangeable name for a type expression.

---

## 2. Value and Reference Types

### 2.1 The value/reference axis and the `#` modifier
Every type is a **value type** unless it is marked with `#`, which makes it a **reference type**. This axis is orthogonal to the *shape* of the body (a product `struct` or a sum `variant`, see §2.5). The value product is `struct`; the reference product is `#struct`. The `#` mark applies only to the **body forms** — `#struct`, `#variant`, and `#enum` (see [`adt.md`](adt.md) §3) — and only where a type is declared (§5.3). A reference type is a **distinct type** from any value type; it reuses only the field layout of its body and otherwise has its own identity, its own constructors, and its own methods (see [`memory.md`](memory.md) §2).

A **value type** is copied on assignment, has no identity, and is *transitively* a value: it may contain only other value types, never a reference-type or `&` field (§2.2, [`memory.md`](memory.md) §2.10). A **reference type** has single ownership and stable identity, follows the rules in [`memory.md`](memory.md) §2, may be aliased through `&`, may hold reference-type and `&` fields, and may recurse. Placement — stack or heap — is an unobservable implementation choice for both kinds (see [`memory.md`](memory.md) §3.5).

```zane
package Graph

type Node = #struct {      // reference type: identity, may hold `&`, may recurse
    _id Int;
    scale Float;
    label String;
}
```

> **Story:** [`stories/types.md`](../stories/types.md#two-axes-instead-of-a-keyword-per-kind) — "Two axes instead of a keyword per kind".
> **Story:** [`stories/types.md`](../stories/types.md#what--actually-changes-and-the-boxing-trap) — "What `#` actually changes, and the boxing trap".

### 2.2 Value types are transitive and mutable in place
A value-type body contains only field declarations, stored inline. A value type **MUST NOT** contain a reference-type or `&` field, and this holds transitively: a value type reachable through a value type must itself be a value type (see [`memory.md`](memory.md) §2.10). The restriction is what makes a value copyable and shareable-by-snapshot with no ownership or anchor bookkeeping.

A value is **mutable in place**: a `mut` method may write its fields, because the receiver is a *borrow* of the caller's storage rather than a copy (see [`effects.md`](effects.md) §2.3 and [`functions.md`](functions.md) §2.4). A value's storage slot may also be overwritten wholesale.

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

The same receiver type written under any other parameter name is a non-receiver parameter and does not gain private-field access.

All fields whose names do not begin with `_` are public.

This is intentional: private-field access in Zane is method-based, not package-based.

### 2.4 Type bodies contain no behavior
Methods, constructors, overload rules, and function values live at package scope. A reader can inspect a type body to learn layout without scanning for behavior.

### 2.5 `struct` and `variant` share one body grammar
A `struct` is a product type: it has all of its members at once. A `variant` is a sum type with the same body grammar: it has exactly one of its members at a time. The body of a `variant` is byte-for-byte the same shape as a `struct` body; the keyword alone flips product into sum.

```zane
type Color = struct { r Int; g Int; b Int; }    // value product: has r and g and b
type Shape = variant { dot Dot; line Line; }      // value sum: has dot or line
```

The `#` modifier (§2.1) is the other axis and applies to both shapes: `struct`/`#struct` are the product pair, `variant`/`#variant` the sum pair. A value type — `struct` or `variant` — is transitively value and **MUST NOT** contain a reference-type field, an `&` field, or recurse (§2.2, [`memory.md`](memory.md) §2.10). A reference type — `#struct` or `#variant` — may hold reference-type and `&` fields and may recurse, boxing recursive members through `&`. The body syntax is symmetric across all four; the keyword picks product versus sum and the `#` picks value versus reference. Because `#` marks only a body form, a reference type comes into being only through such a declaration and is always named there (§5.3).

> **Story:** [`stories/types.md`](../stories/types.md#confining--to-the-body-forms) — "Confining `#` to the body forms".

> **See also:** [`adt.md`](adt.md) for the canonical rules on `variant`, `enum`, pattern matching, and enum maps. [`adt.md`](adt.md) §3 for the full struct-versus-variant symmetry.

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

### 3.4 Implicit field access in constructor calls
Field-constructor call sites may use implicit field access when the argument expression name matches the field name:

```zane
x Int(3)
y Int(2)
vec Vector{x, y}
```

`Vector{x, y}` is shorthand for `Vector{x = x, y = y}`.

### 3.5 Implicit field access in `init{ }`
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

### 3.6 `init{ }` is a constructor-only expression
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

### 3.7 Constructors do not use `mut`
Constructors are not methods. They create new values rather than mutating an existing receiver, so `mut` does not apply.

### 3.8 `&` fields require `&` constructor parameters
An `&` field is legal only in a reference type (`#struct`/`#variant`), since a value type is transitively value (§2.2). A constructor that assigns a value to an `&` field must declare the corresponding parameter as `&T`. The caller must then supply a source that may create a new `&` under [`memory.md`](memory.md) §2.8 — not a temporary or `[]` expression.

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
engine Engine()
car Car(engine)   // legal: engine may create a new `&`
```

```zane
car Car(Engine())   // ILLEGAL: temporary cannot initialize an `&` field
```

A reference type whose fields are all plain owners does not require `&` parameters:

```zane
type Car = #struct {
    engine Engine;
}

Car(engine Engine) {
    return init{engine = engine}
}

car Car(Engine())   // legal: plain owner field accepts a temporary
```

### 3.9 Type and number parameters
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

Void printDistance(d Meters) { ... }
```

At a **coercion site** — a positional argument of a function or constructor call, or a named field entry of a field-constructor call (see §4.2) — if the source expression has a different type from the parameter or field and exactly one applicable implicit constructor exists, the compiler inserts that constructor call automatically.

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
A coercion site is a position that passes a value into a **call or constructor** whose corresponding parameter or field type is known. These are the only positions where the compiler inserts an implicit constructor:

- Positional arguments of a function call
- Positional arguments of a method call (the receiver is excluded; see §4.6)
- Positional arguments of a positional constructor call `Type(...)`
- Named field entries of a field-constructor call `Type{ field = expr }`

A field-constructor call entry fills the constructor's declared slot, exactly as a positional argument fills a slot whose type is fixed by the callee's signature, so the two coerce alike.

An implicit constructor is **never** inserted at any other position. In particular, the following are **not** coercion sites:

- Symbol declarations with a type annotation: `name VarType = expr`
- Assignments to already-declared symbols: `name = expr`
- Field or subscript assignments: `obj.field = expr`, `arr[i] = expr`
- `return` expressions, even when the return type is declared
- Named field entries of an `init{ field = expr }` initializer inside a constructor body

At each of these positions the destination type is one you fix yourself — a local declaration, existing storage, the return type in the enclosing signature, or the fields the constructor builds through `init{ }` — rather than a contract you pass a value into, so the conversion must be written explicitly.

Operator operands **are** coercion sites, because operators desugar to ordinary function calls (see [operators.md](operators.md) §2.2); each operand is a positional argument of that call.

At one coercion site requiring destination type `T` (a parameter or field type), given an argument with static type `U`, the compiler resolves the site locally:

1. If `U` is exactly `T`, accept the argument with no insertion.
2. Otherwise, collect all visible applicable implicit constructors from `U` to `T`.
3. If exactly one applicable implicit constructor exists, rewrite the argument as `T(arg)`.
4. If multiple applicable implicit constructors exist, the site is an ambiguity error.
5. If none exist, the site is a normal type error.

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
An implicit constructor from type `U` to type `T` **MUST** be declared in the home package of either `T` or `U`. A third-party package **MUST NOT** declare an implicit constructor between two imported types.

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

### 4.6 Method receivers are never implicitly converted
The receiver expression (`this`) in a method call is never subject to implicit conversion. This remains true even though method calls desugar to ordinary function calls. If the receiver type does not match, the call is a type error.

```zane
Void logDistance(this Meters) { ... }

feet Feet(Float(10))
feet:logDistance()   // ILLEGAL: receiver type is Feet, not Meters
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
The right-hand side of a `type` or `alias` declaration is any type expression: an applied generic (`Vector<Int>`), an `Array<Int, 10000>`, a `#`-marked reference body, or an inline `struct { ... }`, `variant { ... }`, `enum [ ... ]`, or `tuple [ ... ]` (and the `#` forms of the bodies).

```zane
type Wrapper = struct {
    vec Vector<Int>;
    arr Array<Int, 10000>;
}
```

A named type is therefore always declared this way: `type Name = struct { ... }` or `type Name = #struct { ... }` (and likewise `variant`/`#variant`/`enum`). There is no standalone `struct Name { ... }` declaration form — the body forms are type expressions that only name a type through a `type` (or `alias`) declaration.

A **type-defining** expression — a `struct`/`variant`/`enum` body, its `#` form, or a `tuple [ ... ]` — **MUST** appear only as the right-hand side of a `type` or `alias` declaration. Every other type position — a field, a parameter, a return type — names a declared type or an instantiation of one (`Weapon`, `Vector<Int>`, `Array<Int, 10000>`, `&Node`). Every constructible type therefore has a name, and that name is what its constructor is called by (§3.1).

> **Story:** [`stories/types.md`](../stories/types.md#every-type-has-a-name-because-construction-needs-one) — "Every type has a name, because construction needs one".

### 5.4 The keyword carries the distinction
Intent lives entirely in the keyword — `type` versus `alias` — not in the punctuation. The `=` delimiter is identical in both forms, which keeps them visually parallel while making the distinct-vs-interchangeable choice explicit.

> **See also:** [`generics.md`](generics.md) §4 for type expressions and [`syntax.md`](syntax.md) §1 for the declaration grammar.

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Type bodies contain fields only | Separates layout from behavior and makes storage inspectable at a glance. |
| One kind axis with the `#` modifier | Value versus reference is orthogonal to product versus sum, so one modifier (`#`) spans `struct`/`variant`/`enum` rather than a separate keyword per combination. `#` marks only a body form: a reference type is a distinct type, declared and named, that reuses only its body's layout. |
| Every type is named | Construction is name-based — a constructor is a verb named after its type — so a type with no name has no constructor. A use-site position therefore names a declared type; type-defining bodies and `tuple[...]` appear only as a declaration right-hand side. |
| Value types cannot contain reference-type or `&` fields | A value is copied inline; embedding an identity-bearing or ref-bearing field would silently duplicate ownership or anchor state. The restriction (checked transitively) is what keeps a value alias-free and safe to snapshot. |
| Value types mutate in place through a borrowed receiver | A `mut` method borrows the caller's storage, so a value is mutated without the return-a-replacement dance and without gaining identity. |
| Name-based field privacy | Privacy follows method receivers, not packages. A method declared in any package gets the same private-field access as one declared in the home package. |
| Constructors are package-scope declarations | Avoids partial-object semantics and keeps construction in the same model as functions and methods. |
| Field constructors, defaults, and `init{}` shorthand | Removes repetitive `field = field` boilerplate when names already match, while still allowing direct field-parameter constructors to supply sensible defaults. |
| Implicit constructors for coercion | Allows ergonomic conversions when passing arguments to function and constructor calls, without operator overloading or hidden multi-step chaining. |
| Implicit conversion at call and constructor arguments | A call, positional-constructor, or field-constructor argument fills a slot whose type is fixed entirely by the callee's signature, so the conversion serves a contract the caller is satisfying — this includes the named field entries of a `Type{field = value}` call. Declarations, assignments, field and subscript stores, `return`, and the `init{field = value}` a constructor writes its own value through instead fix the destination type themselves — a local declaration, existing storage, the enclosing return type, or the constructor's own fields — rather than adapting to a contract, where a hidden conversion would be surprising, so they stay explicit. |
| Single-parameter requirement for implicit constructors | Keeps conversion semantics unambiguous: one source value produces one destination value. |
| No field-constructor form for implicit constructors | Field constructors name their parameters after fields; implicit constructors name their parameter after the source type. The forms serve different purposes. |
| No chaining of implicit conversions | Prevents hidden complexity and keeps conversion cost bounded and predictable. |
| Source type must be a value type or compiler concept | Reference types have ownership and identity; implicitly converting one would hide ownership transfer. Compiler concept types in `@concepts$...` are designed for ergonomic lowering. |
| Coherence: orphan rule for implicit constructors | Prevents third-party packages from introducing conflicting conversions between types they do not own. |
| Method receivers never implicitly converted | Preserves dispatch clarity: the method is selected by the receiver's actual type, not by a conversion that happens to make the call legal. |
| `type` vs `alias` keywords | The choice between a new distinct type and an interchangeable alias lives in the keyword, not in a single mid-declaration character, so intent is unambiguous at a glance. |
| `Type` / `Number` constructor parameters | A constructor call carries no `<>` list, so a parameterized type's constructor receives its template parameters as ordinary arguments — inferred from inline introduction (on a value parameter's type or a nested type) or passed explicitly as `Type`/`Number` value parameters. Reusing the concept-type machinery avoids a bespoke parameter-kind keyword. |

---

## 7. Summary

| Concept | Rule |
|---|---|
| Type body | Fields only — no methods or constructors inside the body |
| Value/reference axis | A type is a value type unless marked `#`; `#` marks only the body forms `#struct`/`#variant`/`#enum` (declared and named), each a distinct reference type with identity, `&`-aliasing, and recursion; `struct`/`variant` are value types |
| Use-site types | A field, parameter, or return type names a declared type or an instantiation (`Weapon`, `Vector<Int>`, `&Node`); type-defining bodies and `tuple[...]` appear only as a `type`/`alias` right-hand side |
| Value type | Copied on assignment; transitively value (no reference-type or `&` field, anywhere downstream); mutable in place through a borrowed `mut` receiver; storage may also be overwritten wholesale |
| Reference type (`#`) | Single ownership and stable identity; may hold reference-type and `&` fields; may recurse; placement is unobservable |
| Field visibility | Names starting with `_` are private to `this`-parameter methods on the receiver type; all other names are public |
| Constructor | Package-scope verb named after the type; the written type name is the return type; no `this`; may use block or `=> init{...}` form |
| Field constructor | Declares field parameters directly, may assign default values, and may use `init{field}` shorthand |
| Implicit constructor | Single-parameter constructor marked `implicit`; inserted automatically at positional arguments of function and constructor calls and at named field entries of a `Type{field = value}` call — never at declarations, assignments, stores, `return`, or the `init{field = value}` inside a constructor body; no field-constructor form; source type must be struct or compiler concept; orphan rule applies |
| `&` constructor parameter | Caller must supply an allowed `&` source; callee may store into `&` fields |
| Plain `T` constructor parameter | Value-only; caller may supply a temporary; callee **MUST NOT** bind it into `&` storage |
| `Type` / `Number` constructor parameter | Accepts a type or a compile-time number; inferred from inline introduction or passed explicitly as a value parameter |
| `type` declaration | Introduces a new distinct type, structurally equal to its right-hand side but not interchangeable with it |
| `alias` declaration | Introduces an interchangeable alternate name for a type expression |
