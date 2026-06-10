# Zane Types

This document specifies Zane's data types: classes, structs, fields, constructors, and the `init{ }` expression. Methods and other behavior live in [`functions.md`](functions.md).

> **See also:** [`memory.md`](memory.md) §2 for ownership rules. [`functions.md`](functions.md) for methods and free functions. [`syntax.md`](syntax.md) §1 and §3 for declaration grammar.

---

## 1. Overview

Zane keeps data layout and construction separate from behavior.

- **`Fields-only type bodies`.** Class and struct bodies declare storage only — no methods or constructors live inside the body.
- **`Two type kinds`.** `class` is heap-allocated with single ownership; `struct` is inline value storage.
- **`Package-scope constructors`.** Construction is a function declaration at package scope; the body builds the value with `init{ }`.
- **`Name-based field privacy`.** A leading `_` makes a field private to methods whose first parameter is `this` for that type.

---

## 2. Classes and Structs

### 2.1 Classes declare heap-allocated storage
A `class` body contains only field declarations. Class instances are heap-allocated and follow the ownership rules in [`memory.md`](memory.md) §2.

```zane
package Graph

class Node {
    _id Int
    scale Float
    label String
}
```

### 2.2 Structs are inline value types
A `struct` body also contains only field declarations, but structs are stored inline. Structs **MUST NOT** contain class fields or `&` fields.

This restriction exists because structs use inline value semantics, while class ownership and ref tracking require runtime identity and anchor bookkeeping.

After construction, a struct's fields are immutable: code cannot mutate a struct instance in place. The storage position that holds a struct value remains overwritable, however, so a symbol, field, or container slot of struct type may later be assigned a replacement struct value.

```zane
package Math

struct Vec2 {
    x Float
    y Float
}

pos Vec2(1, 2)
pos = Vec2(3, 4)   // legal: replaces the whole value
```

### 2.3 Field visibility is name-based
Fields whose names begin with `_` are private to methods whose first parameter is `this` for that type, regardless of which package declares the method.

The same receiver type written under any other parameter name is a free-function parameter and does not gain private-field access.

All fields whose names do not begin with `_` are public.

This is intentional: private-field access in Zane is method-based, not package-based.

### 2.4 Type bodies contain no behavior
Methods, constructors, overload rules, and function values live at package scope. A reader can inspect a type body to learn layout without scanning for behavior.

---

## 3. Constructors and Initialization

### 3.1 Constructors are package-scope declarations
A constructor is a package-scope function declaration named after the type. It has no `this` parameter because no object exists yet.

Constructors use the same block-bodied or expression-bodied surface forms as other package-scope functions, except that the written type name is the return type and the body constructs the result with `init{ ... }`.

### 3.2 Positional constructors
Positional constructors declare ordinary parameters and return `init{ ... }`.

```zane
package Graph

Node(id Int, scale Float, label String) {
    return init{
        _id: id,
        scale: scale,
        label: label
    }
}
```

Expression-bodied constructors are equivalent:

```zane
package Math

Vec2(x Float, y Float) => init{x, y}
```

Positional constructors **MAY** be overloaded by arity or parameter types.

Constructor parameters do not need to repeat private-field underscores. It is normal to map a public-facing parameter such as `id` into a private field such as `_id` explicitly inside `init{ }`.

### 3.3 Field constructors
A constructor may also declare fields directly in its parameter header:

```zane
package Math

struct Vector {
    x Int
    y Int
}

Vector{x Int, y Int} {
    return init{x, y}
}
```

This form is the canonical constructor syntax when the constructor parameters map directly to fields.

Field-constructor entries may also declare default values. They use the same initialized declaration forms as ordinary storage declarations. A call may omit any field whose constructor entry provides one:

```zane
class Weapon {
    name String
    fireRate Float
    damage Float
}

Weapon{
    name String = "Pistol",
    fireRate Float(1),
    damage Float = 10
} {
    return init{name, fireRate, damage}
}

starter Weapon{fireRate: Float(2)}
```

### 3.4 Implicit field access in constructor calls
Field-constructor call sites may use implicit field access when the argument expression name matches the field name:

```zane
x Int(3)
y Int(2)
vec Vector{x, y}
```

`Vector{x, y}` is shorthand for `Vector{x: x, y: y}`.

### 3.5 Implicit field access in `init{ }`
Inside `init{ }`, a bare field name is shorthand for `fieldName: fieldName` when a symbol of that name is in scope:

```zane
Vector{x Int, y Int} {
    return init{x, y}
}
```

This is shorthand for:

```zane
Vector{x Int, y Int} {
    return init{
        x: x,
        y: y
    }
}
```

### 3.6 `init{ }` is a constructor-only expression
`init{ }` is valid only inside a constructor body, but within that body it is an ordinary expression of the enclosing constructor's type. It may be returned directly or assigned to a local before being returned.

```zane
Vector{x Int, y Int} {
    temp Vector = init{
        x: x,
        y: y
    }
    return temp
}
```

Every field of the target type **MUST** be assigned exactly once, either explicitly or through implicit field access shorthand.

### 3.7 Constructors do not use `mut`
Constructors are not methods. They create new values rather than mutating an existing receiver, so `mut` does not apply.

### 3.8 `&` fields require `&` constructor parameters
A constructor that assigns a value to an `&` field must declare the corresponding parameter as `&T`. The caller must then supply a source that may create a new `&` under [`memory.md`](memory.md) §2.8 — not a temporary or `[]` expression.

```zane
package Vehicle

class Car {
    engine &Engine
}

// legal: `&` parameter allows storing into `&` field
Car(engine &Engine) {
    return init{engine: engine}
}
```

```zane
// ILLEGAL: plain parameter cannot be bound into `&` storage
Car(engine Engine) {
    return init{engine: engine}   // ERROR: plain parameter MUST NOT be bound into `&` storage
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

A class whose fields are all plain owners does not require `&` parameters:

```zane
class Car {
    engine Engine
}

Car(engine Engine) {
    return init{engine: engine}
}

car Car(Engine())   // legal: plain owner field accepts a temporary
```

---

## 4. Implicit Constructors

### 4.1 The `implicit` modifier
A constructor marked with the `implicit` modifier declares a single-parameter conversion. Implicit constructors **MUST** declare exactly one parameter and **MUST NOT** use field-constructor form.

```zane
package Units

struct Meters {
    value Float
}

struct Feet {
    value Float
}

// implicit conversion from Feet to Meters
implicit Meters(feet Feet) => init{value: feet.value * Float(0.3048)}
```

At a **coercion site** where the destination type is already known, if the source expression has a different type and exactly one applicable implicit constructor exists, the compiler inserts that constructor call automatically.

```zane
distance Meters = Feet(Float(10))   // coercion site: Meters expected, Feet provided
// desugars to: distance Meters = Meters(Feet(Float(10)))
```

### 4.2 Coercion sites
A coercion site is a position where the destination type is already known:
- Symbol declarations with an explicit type annotation: `name Type = expr`
- Assignments to already-declared symbols: `name = expr`
- Field or subscript assignments where the storage type is already known
- Constructor arguments where the parameter type is known
- Method and free-function arguments where the parameter type is known
- `return` expressions in a declaration with an explicit return type

At one coercion site requiring destination type `T`, given an expression with static type `U`, the compiler resolves the site locally:

1. If `U` is exactly `T`, accept the expression with no insertion.
2. Otherwise, collect all visible applicable implicit constructors from `U` to `T`.
3. If exactly one applicable implicit constructor exists, rewrite the expression as `T(expr)`.
4. If multiple applicable implicit constructors exist, the site is an ambiguity error.
5. If none exist, the site is a normal type error.

### 4.3 No chaining
Implicit conversions are never chained. If no single-step implicit constructor exists from source type `U` to destination type `T`, the compiler does not search for a path `U → V → T`. The call is a type error.

### 4.4 Source and destination type constraints
The **source type** (parameter type) of an implicit constructor **MUST** be a struct or a compiler concept type in the `@concepts$` namespace. It **MUST NOT** be a class or an `&`.

The **destination type** (return type, i.e., the type name of the constructor) **MAY** be a struct or a class.

```zane
struct Celsius { value Float }
struct Fahrenheit { value Float }

implicit Celsius(f Fahrenheit) => init{value: (f.value - Float(32)) * Float(5) / Float(9)}   // legal: struct → struct
```

```zane
class Logger { verbosity Int }
struct LogConfig { verbosity Int }

implicit Logger(cfg LogConfig) {   // legal: struct → class
    return init{verbosity: cfg.verbosity}
}
```

```zane
class Source { data String }
class Destination { payload String }

implicit Destination(s Source) {   // ILLEGAL: source type is a class
    return init{payload: s.data}
}
```

### 4.5 Coherence and the orphan rule
An implicit constructor from type `U` to type `T` **MUST** be declared in the home package of either `T` or `U`. A third-party package **MUST NOT** declare an implicit constructor between two imported types.

This rule prevents conflicts when multiple packages independently define the same implicit conversion and ensures that the owner of at least one type controls the conversion behavior.

```zane
package Units

struct Meters { value Float }

// legal: declared in home package of Meters
implicit Meters(feet Feet) => init{value: feet.value * Float(0.3048)}
```

```zane
package Conversions
import Units

// ILLEGAL: neither Meters nor Feet is defined in Conversions
implicit Units$Meters(feet Units$Feet) => init{value: feet.value * Float(0.3048)}
```

### 4.6 Method receivers are never implicitly converted
The receiver expression (`this`) in a method call is never subject to implicit conversion. This remains true even though method calls desugar to ordinary function calls. If the receiver type does not match, the call is a type error.

```zane
Void logDistance(this Meters) { ... }

feet Feet(Float(10))
feet:logDistance()   // ILLEGAL: receiver type is Feet, not Meters
```

> **See also:** [`functions.md`](functions.md) §5 for how implicit constructors interact with overload resolution.

---

## 5. Design Rationale

| Decision | Rationale |
|---|---|
| Type bodies contain fields only | Separates layout from behavior and makes storage inspectable at a glance. |
| Two type kinds (class and struct) | Class instances need single ownership and stable identity for refs; structs are pure inline values with replacement semantics. The split keeps each kind simple. |
| Structs cannot contain class or `&` fields | Structs are plain value storage; allowing them to embed identity-bearing or ref-bearing fields would force them to participate in ownership and anchor bookkeeping. |
| Structs update by replacement, not in-place mutation | Preserves plain value semantics for structs while still allowing ordinary reassignment of struct-typed storage. |
| Name-based field privacy | Privacy follows method receivers, not packages. A method declared in any package gets the same private-field access as one declared in the home package. |
| Constructors are package-scope declarations | Avoids partial-object semantics and keeps construction in the same model as functions and methods. |
| Field constructors, defaults, and `init{}` shorthand | Removes repetitive `field: field` boilerplate when names already match, while still allowing direct field-parameter constructors to supply sensible defaults. |
| Implicit constructors for coercion | Allows ergonomic conversions at assignment sites without operator overloading or hidden multi-step chaining. |
| Single-parameter requirement for implicit constructors | Keeps conversion semantics unambiguous: one source value produces one destination value. |
| No field-constructor form for implicit constructors | Field constructors name their parameters after fields; implicit constructors name their parameter after the source type. The forms serve different purposes. |
| No chaining of implicit conversions | Prevents hidden complexity and keeps conversion cost bounded and predictable. |
| Source type must be struct or compiler concept | Classes have ownership and identity; implicitly converting a class would hide ownership transfer. Compiler concept types in `@concepts$...` are designed for ergonomic lowering. |
| Coherence: orphan rule for implicit constructors | Prevents third-party packages from introducing conflicting conversions between types they do not own. |
| Method receivers never implicitly converted | Preserves dispatch clarity: the method is selected by the receiver's actual type, not by a conversion that happens to make the call legal. |

---

## 6. Summary

| Concept | Rule |
|---|---|
| Class body | Fields only — no methods or constructors inside the body |
| Struct | Inline value type; cannot contain class or `&` fields; fields are immutable after construction, but struct-typed storage may be overwritten |
| Field visibility | Names starting with `_` are private to `this`-parameter methods on the receiver type; all other names are public |
| Constructor | Package-scope function declaration named after the type; the written type name is the return type; no `this`; may use block or `=> init{...}` form |
| Field constructor | Declares field parameters directly, may assign default values, and may use `init{field}` shorthand |
| Implicit constructor | Single-parameter constructor marked `implicit`; inserted automatically at coercion sites; no field-constructor form; source type must be struct or compiler concept; orphan rule applies |
| `&` constructor parameter | Caller must supply an allowed `&` source; callee may store into `&` fields |
| Plain `T` constructor parameter | Value-only; caller may supply a temporary; callee **MUST NOT** bind it into `&` storage |
