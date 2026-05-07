# Zane Object-Oriented Model

This document specifies Zane's object model: classes, structs, constructors, methods, free functions, packages, and function values.

> **See also:** [`memory_model.md`](memory_model.md) §2 for ownership rules. [`purity.md`](purity.md) §2 for `mut`. [`syntax.md`](syntax.md) §1 and §3 for declaration grammar.

---

## 1. Overview

Zane keeps data layout, construction, and behavior as separate but composable concepts.

- **`Fields-only type bodies`.** Class and struct bodies declare storage only.
- **`Package-scope behavior`.** Constructors, methods, and free functions are declared at package scope.
- **`Explicit mutation at the call site`.** `:` calls are read-only; `!` calls invoke `mut` methods.
- **`Methods as functions`.** A method is a function whose first parameter is `this`, so methods and function values share one model.

---

## 2. Classes and Structs

### 2.1 Classes declare heap-allocated storage
A `class` body contains only field declarations. Class instances are heap-allocated and follow the ownership rules in [`memory_model.md`](memory_model.md) §2.

```zane
package Graph

class Node {
    _id Int
    scale Float
    label String
}
```

### 2.2 Structs are inline value types
A `struct` body also contains only field declarations, but structs are stored inline. Structs **MUST NOT** contain class fields or `ref` fields.

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

### 3.6 Constructor completeness rules
`init{ }` is valid only as a constructor return value. Every field of the target type **MUST** be assigned exactly once, either explicitly or through implicit field access shorthand.

### 3.7 Constructors do not use `mut`
Constructors are not methods. They create new values rather than mutating an existing receiver, so `mut` does not apply.

### 3.8 `ref` fields require `ref` constructor parameters
A constructor that assigns a value to a `ref` field must declare the corresponding parameter as `ref T`. The caller must then supply a place expression (a named symbol, field access, or subscript projection) — not a temporary.

```zane
package Vehicle

class Car {
    engine ref Engine
}

// legal: ref parameter allows storing into ref field
Car(engine ref Engine) {
    return init{engine: engine}
}
```

```zane
// ILLEGAL: plain parameter cannot be bound into ref storage
Car(engine Engine) {
    return init{engine: engine}   // ERROR: plain parameter MUST NOT be bound into ref storage
}
```

Call sites:

```zane
engine Engine()
car Car(engine)   // legal: engine is a place expression
```

```zane
car Car(Engine())   // ILLEGAL: temporary cannot initialize a ref field
```

A class whose fields are all plain owners does not require `ref` parameters:

```zane
class Car {
    engine Engine
}

Car(engine Engine) {
    return init{engine: engine}
}

car Car(Engine())   // legal: plain owner field accepts a temporary
```

### 3.9 Implicit constructors
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

#### 3.9.1 Coercion sites
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

#### 3.9.2 Overload resolution with implicit constructors
For free-function calls, constructor calls, and desugared method calls, overload resolution proceeds in three phases:

1. **Direct match.** A candidate is viable only if the call type-checks with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
2. **Generic match.** If the direct phase finds no viable candidate, type parameters may be chosen or inferred, still with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
3. **Implicit match.** If the direct and generic phases find no viable candidate, implicit constructors may be inserted at coercion sites. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.

If no phase yields a viable candidate, the call is a normal no-match type error.

Implicit constructors are therefore a last resort. They never participate in discovering an otherwise unknown destination type for generic inference.

#### 3.9.3 No chaining
Implicit conversions are never chained. If no single-step implicit constructor exists from source type `U` to destination type `T`, the compiler does not search for a path `U → V → T`. The call is a type error.

#### 3.9.4 Source and destination type constraints
The **source type** (parameter type) of an implicit constructor **MUST** be a struct or a compiler concept type in the `@concepts$` namespace. It **MUST NOT** be a class or a `ref`.

The **destination type** (return type, i.e., the type name of the constructor) **MAY** be a struct or a class.

```zane
struct Celsius { value Float }
struct Fahrenheit { value Float }

implicit Celsius(f Fahrenheit) => init{value: (f.value - Float(32)) * Float(5) / Float(9)}   // legal: struct → struct
```

```zane
class Logger { message String }
struct LogConfig { verbosity Int }

implicit Logger(cfg LogConfig) {   // legal: struct → class
    return init{message: cfg.verbosity:toString()}
}
```

```zane
class Source { data String }
class Destination { payload String }

implicit Destination(s Source) {   // ILLEGAL: source type is a class
    return init{payload: s.data}
}
```

#### 3.9.5 Coherence and the orphan rule
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
import units

// ILLEGAL: neither Meters nor Feet is defined in Conversions
implicit units$Meters(feet units$Feet) => init{value: feet.value * Float(0.3048)}
```

#### 3.9.6 Method receivers are never implicitly converted
The receiver expression (`this`) in a method call is never subject to implicit conversion. This remains true even though method calls desugar to ordinary function calls. If the receiver type does not match, the call is a type error.

```zane
Void logDistance(this Meters) { ... }

feet Feet(Float(10))
feet:logDistance()   // ILLEGAL: receiver type is Feet, not Meters
```

---

## 4. Methods

### 4.1 Methods are functions whose first parameter is `this`
A method is any package-scope function whose first parameter is named `this`. `this` **MUST** be the first parameter and **MUST NOT** appear in any other parameter position.

```zane
Int scaledId(this Node, factor Int) {
    return this._id * factor
}
```

### 4.2 `this` grants private-field access
Naming the first parameter `this` is the only thing that makes a declaration a method. That token grants access to `_`-prefixed fields on the receiver type regardless of which package declares the method; home-package status does not matter. The same parameter type written with another name is a free function and does not grant private-field access.

```zane
Int scaledId(this Node, factor Int) {
    return this._id * factor
}

Int scaledIdWrong(node Node, factor Int) {
    return node._id * factor   // ILLEGAL: node is not `this`
}
```

### 4.3 Read-only methods are the default
A method without `mut` may read `this`, its parameters, and reachable read-only state, but it may not write to `this` or owned descendants.

### 4.4 Mutating methods use `mut`
A method marked `mut` may write to `this` and objects owned by `this`.

The receiver of a `mut` method **MUST** be a class type. Struct receivers are value receivers, so their fields cannot be changed in place. Code that wants an updated struct returns a replacement value instead of mutating the original.

```zane
Void setScale(this Node, scale Float) mut {
    this.scale = scale
}
```

```zane
Vec2 setY(this Vec2, y Float) => Vec2(this.x, y)

pos = pos:setY(Float(3))
```

### 4.5 Call markers are part of the surface syntax
Read-only methods are called with `:`. Mutating methods are called with `!`.

```zane
node:scaledId(Int(2))
node!setScale(Float(3))
```

Calling a `mut` method with `:` is illegal. Calling a non-`mut` method with `!` is also illegal.

### 4.6 Method desugaring

```
receiver:method(arg)        → ResolvedPkg$method(receiver, arg)
receiver!method(arg)        → ResolvedPkg$method(receiver, arg)
receiver:Pkg$method(arg)    → Pkg$method(receiver, arg)
receiver!Pkg$method(arg)    → Pkg$method(receiver, arg)
```

### 4.7 Parameters are read-only
Explicit parameters other than `this` are read-only. Mutation of another object must be expressed as a `mut` method call on that object as the receiver.

### 4.8 `ref` method parameters
A method parameter declared as `ref T` requires the caller to supply a place expression and permits the callee to store that argument into a `ref` field. A parameter declared as plain `T` is value-only. A plain `T` parameter does not guarantee a stable ref-able source location, therefore it MUST NOT be bound into `ref` storage.

```zane
class Car {
    engineA ref Engine
    engineB Engine
}

// legal: engine is ref-capable; may be stored in either field
Void consume(this Car, engine ref Engine) mut {
    this.engineA = engine   // legal
    this.engineB = engine   // legal
}

// legal: engine is value-only; may only be read or moved into plain fields
Int calculate(this Car, engine Engine) {
    return this._value + engine.speed   // legal: reading only
}

// ILLEGAL: plain parameter stored into ref field
Void consumeWrong(this Car, engine Engine) mut {
    this.engineA = engine   // ILLEGAL
}
```

Call syntax is uniform regardless of whether a parameter is `ref`:

```zane
engine Engine()
car!consume(engine)        // legal: engine is a place expression
car:calculate(engine)      // legal: place expression works for plain T too
car!consume(Engine())      // ILLEGAL: temporary cannot bind to ref parameter
car:calculate(Engine())    // legal: plain T parameter accepts a temporary
```

### 4.9 Subscripts are place projections
Subscripts are package-scope declarations with the receiver first:

```zane
(this CustomList)[index Int] => this._data[index]
(this Tensor3)[x Int, y Int, z Int] => this._data[x][y][z]
```

The body of a subscript definition **MUST** be a place expression. `[]` is not a general function call and cannot return a computed value. Its result is always inferred from the projected place, so subscripts have no explicit return type annotation. A subscript may declare any number of comma-separated parameters inside `[]`; it is not limited to one or two.

```zane
value ref Int = list[i]     // legal when list is a place
```

```zane
(this CustomList)[index Int] => this._data.compute(index)   // ILLEGAL: body is not a place expression
Int (this CustomList)[index Int] => this._data[index]       // ILLEGAL: explicit return type not allowed
```

`list[i]` is a place expression only if `list` is a place expression. `CustomList()[0]` is therefore not a place expression because the base is a temporary.

---

## 5. Free Functions

### 5.1 Free functions are package-scope functions without `this`
A free function is any package-scope function whose first parameter is not named `this`.

```zane
Float getScale(node Node) {
    return node.scale
}
```

### 5.2 Free functions cannot access private fields
Free functions may access only fields whose names do not begin with `_`. This rule is package-independent: a free function declared in the same package as the type still cannot access `_`-prefixed fields unless its first parameter is named `this`.

### 5.3 Free functions use ordinary call syntax
Free functions are called as `name(args...)` or `Package$name(args...)`.

### 5.4 Expression-bodied functions and methods
Both free functions and methods may use `=>` when they return a value:

```zane
Int double(value Int) => value * 2
Int scaledId(this Node, factor Int) => this._id * factor
```

`=> expr` desugars to `{ return expr }`. Because the shorthand always returns its expression, it is illegal for declarations whose return type is `Void`.

---

## 6. Overloading Rules

### 6.1 Overload identity is parameter types only
Two declarations in the same package conflict when they have the same ordered parameter types. Parameter names, `this`, `mut`, and return type do not distinguish overloads.

Two overloads **MUST NOT** differ only by whether the same parameter position is `T` versus `ref T`. Such declarations are illegal and the compiler **MUST** reject them with a compile-time error, for example: "illegal overload set: differs only by `ref` on a parameter; rename one declaration or choose a single signature."

```zane
Void consume(this Car, engine Engine)
Void consume(this Car, engine ref Engine)  // ERROR
```

### 6.2 Consequences of the overload identity rules
Declarations that differ only by return type, parameter names, `this`, or `mut` are compile-time conflicts.

### 6.3 Valid overloads differ by arity or parameter type
Legal overload sets must differ in the number of parameters or in at least one parameter type other than bare `ref`-ness at the same position.

---

## 7. Function Values and Lambdas

### 7.1 Package-scope functions are referenced as `Package$name`
Methods and free functions are both referenced as values using the package namespace:

```zane
Graph$scaledId
Graph$setScale
Graph$getScale
```

When referenced as a value, a method's `this` parameter remains explicit in the function type.

### 7.2 Lambdas are explicitly typed
Lambda declarations use an explicit function type and a lambda literal with a matching parameter list:

```zane
(this Node, Int) mut -> Void callback = (this Node, Int) mut {
    ...
}
```

The lambda literal repeats the parameter list and `mut` marker. A non-`mut` lambda omits `mut` in both places.

### 7.3 Lambdas do not capture
Lambdas **MUST NOT** capture outer variables. Every dependency must be passed as a parameter or supplied through surrounding storage explicitly. See [`concurrency_model.md`](concurrency_model.md) §5.2 ("Lambdas do not capture").

### 7.4 No bound method references
Zane does not provide bound method references as a separate feature. Because lambdas do not capture, there is no syntax that implicitly stores a receiver inside a function value. Code that needs a receiver later must keep that receiver in ordinary storage and pass it explicitly when the function value is invoked.

---

## 8. Method Name Resolution and Extension Methods

### 8.1 Unqualified method lookup
For `receiver:methodName(...)` or `receiver!methodName(...)`, the compiler resolves candidates in this order:

1. the receiver type's home package
2. the current package

If no candidate matches, the call is a compile-time error. If multiple candidates remain after overload resolution, the call is a compile-time error and must be written with an explicit package qualifier.

### 8.2 Qualified method calls
Cross-package extension methods are written explicitly:

```zane
vec:Physics$kineticEnergy()
```

### 8.3 Extension methods may be declared in any package
Because methods are package-scope functions, any package may define methods on imported types. This follows the same rule as §2.3 and §4.2: if the first parameter is `this`, the declaration is a method and gets the same private-field access as any other method on that receiver type.

---

## 9. Packages and the Instanceful Package Pattern

### 9.1 Packages are namespaces
`package X` introduces a namespace. Members are referenced as `X$member`.

### 9.2 A package may define a same-named class
A package may define a class with the same name as the package, allowing both stateless namespace members and stateful instances:

```zane
package Math

pi Float(3.141592)

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}

class Math {
    logger Logger
}
```

### 9.3 Package scope contains no hidden mutable ambient state
Packages may expose immutable constants and package-scope functions. Time-varying state must live in objects and be passed or stored explicitly.

---

## 10. Connection to the Effect Model

Read-only methods and free functions are effect-free with respect to their receiver unless they touch refs or capabilities. `mut` marks the only direct path for writing receiver-owned state. This is why overload identity ignores `mut`: the call contract is structurally the same even though the behavioral permissions differ.

> **See also:** [`purity.md`](purity.md) for the complete effect model and concurrency implications.

---

## 11. Design Rationale

| Decision | Rationale |
|---|---|
| Type bodies contain fields only | Separates layout from behavior and makes storage inspectable at a glance. |
| Constructors are package-scope declarations | Avoids partial-object semantics and keeps construction in the same model as functions and methods. |
| Structs update by replacement, not in-place mutation | Preserves plain value semantics for structs while still allowing ordinary reassignment of struct-typed storage. |
| Field constructors and `init{}` shorthand | Removes repetitive `field: field` boilerplate when names already match, while keeping field assignment explicit in structure. |
| Implicit constructors for coercion | Allows ergonomic conversions at assignment sites without operator overloading or hidden multi-step chaining. |
| Single-parameter requirement for implicit constructors | Keeps conversion semantics unambiguous: one source value produces one destination value. |
| No field-constructor form for implicit constructors | Field constructors name their parameters after fields; implicit constructors name their parameter after the source type. The forms serve different purposes. |
| Overload resolution phases: direct, generic, implicit | Makes implicit conversions a fallback after exact matches, preventing surprising behavior when an exact match exists. |
| No chaining of implicit conversions | Prevents hidden complexity and keeps conversion cost bounded and predictable. |
| Source type must be struct or compiler concept | Classes have ownership and identity; implicitly converting a class would hide ownership transfer. Compiler concept types in `@concepts$...` are designed for ergonomic lowering. |
| Coherence: orphan rule for implicit constructors | Prevents third-party packages from introducing conflicting conversions between types they do not own. |
| Method receivers never implicitly converted | Preserves dispatch clarity: the method is selected by the receiver's actual type, not by a conversion that happens to make the call legal. |
| Methods are functions with `this` | Keeps the language model flat: methods are ordinary functions with one extra permission token. |
| `ref` parameters in constructors and methods | A `ref` field must be initialized from a place expression; requiring `ref` on the corresponding parameter makes this constraint visible in the signature without ghost refs or hidden storage creation. |
| Plain `T` parameters are value-only | A caller is not required to supply a stable storage location for a plain parameter; restricting plain parameters from populating `ref` fields prevents hidden dependency on call-site expression form. |
| `:` and `!` are distinct call markers | Makes mutation visible at the call site without adding mutable-reference types. |
| No overloads that differ only by `ref` | Call syntax stays uniform while avoiding overload ambiguity between value-only and place-required contracts. |
| Package-qualified function values | Uses one naming rule for methods and free functions. |
| No lambda capture | Preserves explicit data flow and keeps effect analysis tractable. |
| Home-package-first method lookup | Makes unqualified method calls locally understandable and unaffected by imports. |
| No hidden mutable package state | Prevents ambient state from undermining ownership and effect reasoning. |

---

## 12. Summary

| Concept | Rule |
|---|---|
| Class body | Fields only — no methods or constructors inside the body |
| Struct | Inline value type; cannot contain class or `ref` fields; fields are immutable after construction, but struct-typed storage may be overwritten |
| Constructor | Package-scope function declaration named after the type; the written type name is the return type; no `this`; may use block or `=> init{...}` form |
| Field constructor | Declares field parameters directly and may use `init{field}` shorthand |
| Implicit constructor | Single-parameter constructor marked `implicit`; inserted automatically at coercion sites; no field-constructor form; source type must be struct or compiler concept; orphan rule applies |
| Overload resolution phases | Direct match, then generic match, then implicit match; ambiguity within any one phase is an error |
| `ref` constructor/method parameter | Caller must supply a place expression; callee may store into `ref` fields |
| Plain `T` constructor/method parameter | Value-only; caller may supply a temporary; callee MUST NOT bind it into `ref` storage |
| Method | Package-scope function whose first parameter is `this` |
| `mut` method | Called with `!`; receiver MUST be a class; may mutate `this` and its owned subtree |
| Free function | Package-scope function without `this`; no private-field privilege |
| Subscript | Package-scope place projection written `(this T)[...] => placeExpr`; no explicit return type |
| Overload identity | Parameter types only; not names, return type, or `mut`; overloads differing only by `ref` at one position are illegal |
| Lambda | Explicitly typed function value; no capture |
| Unqualified method lookup | Searches home package, then current package |
