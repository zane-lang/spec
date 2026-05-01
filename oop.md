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

```zane
package Math

struct Vec2 {
    x Float
    y Float
}
```

### 2.3 Field visibility is name-based
Fields whose names begin with `_` are private to methods in the defining package. All other fields are public.

### 2.4 Type bodies contain no behavior
Methods, constructors, overload rules, and function values live at package scope. A reader can inspect a type body to learn layout without scanning for behavior.

---

## 3. Constructors and Initialization

### 3.1 Constructors are package-scope declarations
A constructor is a package-scope declaration named after the type. It has no `this` parameter because no object exists yet.

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
A constructor that assigns a value to a `ref` field must declare the corresponding parameter as `ref T`. The caller must then supply a place expression (a named symbol, field, or container element) — not a temporary.

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

---

## 4. Methods

### 4.1 Methods are functions whose first parameter is `this`
A method is any package-scope function whose first parameter is named `this`. `this` **MUST** be the first parameter.

```zane
Int scaledId(this Node, factor Int) {
    return this._id * factor
}
```

### 4.2 `this` grants private-field access
Within the defining package, `this` grants access to `_`-prefixed fields. The same parameter type written with another name is a free-function parameter and does not grant private-field access.

### 4.3 Read-only methods are the default
A method without `mut` may read `this`, its parameters, and reachable read-only state, but it may not write to `this` or owned descendants.

### 4.4 Mutating methods use `mut`
A method marked `mut` may write to `this` and objects owned by `this`.

```zane
Void setScale(this Node, scale Float) mut {
    this.scale = scale
}
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
A method parameter declared as `ref T` requires the caller to supply a place expression and permits the callee to store that argument into a `ref` field. A parameter declared as plain `T` is value-only and may not be stored into a `ref` field.

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
Free functions may access only public fields unless they call a method that has the necessary package-local privileges.

### 5.3 Free functions use ordinary call syntax
Free functions are called as `name(args...)` or `Package$name(args...)`.

---

## 6. Overloading Rules

### 6.1 Overload identity is parameter types only
Two declarations in the same package conflict when they have the same ordered parameter types. Parameter names, `this`, `mut`, and return type do not distinguish overloads.

`ref T` and `T` are distinct parameter types for overload resolution purposes.

### 6.2 Consequences of parameter-type-only identity
Declarations that differ only by return type, parameter names, `this`, or `mut` are compile-time conflicts.

### 6.3 Valid overloads differ by arity or parameter type
Legal overload sets must differ in the number of parameters or in at least one parameter type.

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
Because methods are package-scope functions, any package may define methods on imported types. Extension methods do not gain access to the target type's private fields unless they are declared in the defining package.

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
| Field constructors and `init{}` shorthand | Removes repetitive `field: field` boilerplate when names already match, while keeping field assignment explicit in structure. |
| Methods are functions with `this` | Keeps the language model flat: methods are ordinary functions with one extra permission token. |
| `ref` parameters in constructors and methods | A `ref` field must be initialized from a place expression; requiring `ref` on the corresponding parameter makes this constraint visible in the signature without hiding storage creation. |
| Plain `T` parameters are value-only | A caller is not required to supply a stable storage location for a plain parameter; restricting plain parameters from populating `ref` fields prevents hidden dependency on call-site expression form. |
| `:` and `!` are distinct call markers | Makes mutation visible at the call site without adding mutable-reference types. |
| Parameter types alone define overloads | Keeps overload resolution simple and predictable. `ref T` and `T` are different types for this purpose. |
| Package-qualified function values | Uses one naming rule for methods and free functions. |
| No lambda capture | Preserves explicit data flow and keeps effect analysis tractable. |
| Home-package-first method lookup | Makes unqualified method calls locally understandable and unaffected by imports. |
| No hidden mutable package state | Prevents ambient state from undermining ownership and effect reasoning. |

---

## 12. Summary

| Concept | Rule |
|---|---|
| Class body | Fields only — no methods or constructors inside the body |
| Struct | Inline value type; cannot contain class or `ref` fields |
| Constructor | Package-scope declaration named after the type; no `this` |
| Field constructor | Declares field parameters directly and may use `init{field}` shorthand |
| `ref` constructor/method parameter | Caller must supply a place expression; callee may store into `ref` fields |
| Plain `T` constructor/method parameter | Value-only; caller may supply a temporary; callee may not store into `ref` fields |
| Method | Package-scope function whose first parameter is `this` |
| `mut` method | Called with `!`; may mutate `this` and its owned subtree |
| Free function | Package-scope function without `this`; no private-field privilege |
| Overload identity | Parameter types only; not names, return type, or `mut`; `ref T` ≠ `T` |
| Lambda | Explicitly typed function value; no capture |
| Unqualified method lookup | Searches home package, then current package |
