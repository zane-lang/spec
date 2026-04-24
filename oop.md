# Zane Object-Oriented Model

This document specifies Zane's object-oriented model: how types are defined, how objects are constructed, how behavior is attached to types, and how packages work as namespaces.

> **See also:** [`purity.md`](purity.md) for the effect model that sits on top of these constructs. [`memory_model.md`](memory_model.md) for the ownership and lifetime rules that govern class instances.

---

## 1. Overview

Zane's OOP model is built on a small number of orthogonal concepts:

- **Classes** define the memory layout of heap-allocated objects (fields only).
- **Constructors** are standalone declarations that describe how to produce a fully initialized instance of a class.
- **Methods** are free functions whose first parameter is named `this`, granting private field access and `:` call syntax.
- **Free functions** operate on values passed explicitly and cannot access private fields.
- **Packages** are namespaces that may optionally define a class of the same name, making the package "instanceful".

These concepts compose with Zane's ownership model (single owner, `ref` for non-owning references) and effect model (`mut` for methods that write to `this`).

---

## 2. Classes and Structs

### 2.1 A class declares fields only

A `class` body contains nothing but field declarations. No methods, no constructors, no logic. This keeps the class definition minimal and readable at a glance. All behavioral declarations live at package scope.

```zane
package Graph

class Node {
    _id Int
    scale Float
}
```

### 2.2 Classes are heap-allocated reference types

Instances of classes live on the heap and are subject to Zane's ownership rules:
- exactly one owner at any time
- non-owning references use `ref`
- destruction is deterministic and propagates down the ownership tree

### 2.3 Structs are value types

Structs follow the same field-declaration-only body rule but are value types: stored on the stack or inline in their containing class or struct. Structs may not contain class fields or `ref` fields.

```zane
package Math

struct Vec2 {
    x Float
    y Float
}
```

### 2.4 Field visibility

Field visibility is controlled by naming convention:
- fields beginning with `_` are **private**: accessible only from methods (functions whose first parameter is `this`) defined in the **same package**
- all other fields are **public**: readable from anywhere via `.` field access

There is no `private` or `public` keyword. The compiler enforces this rule mechanically based on the parameter name.

```zane
package Graph

class Node {
    _id Int      // private: only accessible via this in package Graph
    scale Float  // public: accessible anywhere via node.scale
}
```

---

## 3. Constructors

### 3.1 Constructors are declared outside the class body

A constructor is a top-level package-scope declaration bound to a type by name. It is syntactically distinct from class definitions, method declarations, and free functions.

### 3.2 Constructors have no `this`

The object does not exist when the constructor runs. There is no implicit `this` inside a constructor body. The constructor's job is to produce a fully initialized instance via `init{ }` and return it.

This is consistent with the method model where `this` is always an explicit, visible parameter. In a constructor, there is simply no `this` at all.

### 3.3 `init{ }`: raw field injection

`init{ }` is the only mechanism for producing a new instance inside a constructor body. It directly injects values into the fields of the type being constructed. It:
- is only valid as a return value inside a constructor body
- does not call any constructor recursively
- requires every field of the type to be assigned

```zane
package Graph

Node(id Int) {
    return init{
        _id: id,
        scale: Float(1)
    }
}
```

If any field is missing from `init{ }`, it is a **compile-time error**.

### 3.4 Positional constructors

Positional constructors:
- may be **overloaded** by arity or parameter types
- may **not** have default parameters (prevents ambiguity with overload resolution)

> See [`syntax.md`](syntax.md) §3.6 for the declaration and call-site grammar.

**Example:**
```zane
package Graph

Node(id Int) {
    return init{
        _id: id,
        scale: Float(1)
    }
}

// overload: differs in arity
Node(id Int, scale Float) {
    return init{
        _id: id,
        scale: scale
    }
}
```

**Call site:**
```zane
package Main
import Graph

Void main() {
    n1 Graph$Node(Int(1))
    n2 Graph$Node(Int(1), Float(2))
}
```

### 3.5 Named constructors

Named constructors:
- may **not** be overloaded (defaults handle optional fields)
- allow fields with defaults to be omitted at the call site

> See [`syntax.md`](syntax.md) §3.7 for the declaration and call-site grammar.

**Example:**
```zane
package Graph

Node {
    id Int,
    scale Float(1)
} {
    return init{
        _id: id,
        scale: scale
    }
}
```

**Call site:**
```zane
package Main
import Graph

Void main() {
    n1 Graph$Node{id: Int(1)}                    // scale uses default
    n2 Graph$Node{id: Int(1), scale: Float(2)}   // override default
}
```

### 3.6 Mixing positional and named is not allowed

```zane
Graph$Node{Int(1), scale: Float(2)}   // compile error: cannot mix positional and named
```

### 3.7 Constructors and `mut`

Constructors are not methods. They produce a new object via `init{ }` rather than operating on an existing `this`. The `mut` concept does not apply to constructors. Every field must be assigned in `init{ }` — this is enforced at compile time and is not optional.

---

## 4. Methods

### 4.1 Methods are free functions with `this` as the first parameter

A method is any package-scope function whose first parameter is named `this`. The name `this` is a **permission token** that:
- grants read (and, if `mut`, write) access to private `_`-prefixed fields of the receiver type
- enables `:` call syntax at the call site

Methods are declared at package scope alongside constructors and free functions. They are not nested inside class bodies.

```zane
package Graph

Int scaledId(this Node, factor Int) {
    return this._id * factor    // ok: this grants access to _id
}
```

### 4.2 Methods vs. free function calls

Methods (functions whose first parameter is `this`) are called with the `:` operator. Free functions are called directly by name. `:` is **only** call syntax — it cannot produce a value or reference.

> See [`syntax.md`](syntax.md) §4 for the full call syntax reference.

### 4.3 Default methods are read-only

A method with no `mut` modifier:
- may read `this` and its parameters
- may not write to `this` or any field of `this`
- may not call `mut` methods on owned fields of `this` (that would indirectly mutate `this`)

```zane
package Graph

Int scaledId(this Node, factor Int) {
    return this._id * factor    // reads this — ok
}
```

### 4.4 `mut` methods

A method marked `mut` may write to `this` and its owned subtree. `mut` is a modifier on the function declaration.

```zane
package Graph

Void setScale(this Node, s Float) mut {
    this.scale = s              // writes this — ok because mut
}
```

`mut` signals "this call may mutate the receiver". The `:` call syntax already restricts mutation to the receiver only; `mut` opts into that permission.

### 4.5 Parameters are always read-only

No function or method may mutate its explicit parameters. If mutation of another object is needed, it must be expressed as a `mut` method call on that object as receiver.

```zane
package Graph

// illegal: cannot mutate a parameter
Void copyScaleTo(this Node, other Node) mut {
    other.scale = this.scale    // compile error: cannot mutate parameter
}

// correct: mutation expressed as mut method call on other
Void copyScaleTo(this Node, other Node) {
    other:setScale(this.scale)  // ok: setScale is a mut method on other
}
```

---

## 5. Free Functions

A free function is any package-scope function whose first parameter is not named `this`. It:
- cannot access private (`_`-prefixed) fields of any type
- is called directly by name or via `Package$functionName`
- is not eligible for `:` call syntax

```zane
package Graph

// free function: cannot access _id
Float getScale(node Node) {
    return node.scale           // ok: scale is public
}
```

```zane
package Main
import Graph

Void main() {
    node Graph$Node(Int(1))
    s Float = Graph$getScale(node)
}
```

---

## 6. Overloading Rules

### 6.1 Overload identity is determined by parameter types only

Two functions in the same package share a name if and only if they have the same sequence of parameter types. The **names** of parameters, including whether the first parameter is called `this`, do not affect overload identity. The return type does not affect overload identity.

This means the following is a **compile-time error** (duplicate signature):

```zane
package Graph

Int doSomething(this Node, number Int) { ... }  // signature: (Node, Int) -> Int
Int doSomething(node Node, number Int) { ... }  // signature: (Node, Int) -> Int
// compile error: identical signatures
```

### 6.2 `mut` does not create a distinct overload

`mut` is a behavioral modifier used for effect analysis. It is not part of the overload signature. Two functions that differ only in `mut` have identical signatures and cannot coexist:

```zane
package Graph

Int doSomething(this Node, number Int) { ... }
Int doSomething(this Node, number Int) mut { ... }
// compile error: mut does not distinguish overloads
```

### 6.3 Legal overloads differ in arity or parameter types

```zane
package Graph

// legal: differ in arity
Int doSomething(this Node, number Int) { ... }
Int doSomething(this Node, a Int, b Int) { ... }

// legal: differ in parameter type at position 2
Int doSomething(this Node, number Int) { ... }
Float doSomething(this Node, number Float) { ... }

// illegal: same parameter types, only return type differs
Int doSomething(this Node, number Int) { ... }
Float doSomething(this Node, number Int) { ... }
// compile error: return type alone does not distinguish overloads

// illegal: mut only
Int doSomething(this Node, number Int) { ... }
Int doSomething(this Node, number Int) mut { ... }
// compile error: mut does not distinguish overloads

// illegal: this vs node only
Int doSomething(this Node, number Int) { ... }
Int doSomething(node Node, number Int) { ... }
// compile error: parameter name does not distinguish overloads
```

---

## 7. Methods and Free Functions as Values

### 7.1 All package-scope functions are referenced via `Package$name`

Methods are free functions that live in the package namespace. There is no special syntax for referencing a method as a value through the type. All package-scope functions — whether methods or free functions — are referenced as values using the `$` namespace separator:

```zane
Graph$scaledId    // type: (Graph$Node, Int) -> Int
Graph$setScale    // type: (this Graph$Node, Float) mut -> Void
Graph$getScale    // type: (Graph$Node) -> Float
```

When used as a value, `this` becomes an explicit first argument. `mut` appears in the function type to signal that calling the reference may mutate the first argument.

> See [`syntax.md`](syntax.md) §4.4 for the full grammar of function references and §2.4 for function type notation.

### 7.2 Passing functions as values

```zane
package Main
import Graph

Void applyToAll(
    nodes List<Graph$Node>,
    fn (Graph$Node, Int) -> Int,
    factor Int
) {
    for node in nodes {
        result Int = fn(node, factor)
    }
}

Void main() {
    nodes List<Graph$Node> = ...

    // method reference via package namespace
    applyToAll(nodes, Graph$scaledId, Int(5))

    // free function reference via package namespace
    applyToAll(nodes, Graph$getScale, Int(5))   // type error if signatures don't match
}
```

### 7.3 No bound method references

Bound method references (where the receiver is pre-captured) are not a built-in feature. If a bound reference is needed, the programmer wraps it explicitly with a lambda:

```zane
package Main
import Graph

Void main() {
    node Graph$Node(Int(1))

    // explicit capture in a lambda
    bound () -> Int = () { node:scaledId(Int(5)) }
    someFunc(bound)
}
```

---

## 8. Extension by Other Packages

Because methods are free functions at package scope, any package can define methods on types imported from other packages. The only restriction is field visibility: extension methods cannot access private (`_`-prefixed) fields, because those are only accessible via `this` in the **defining package**.

```zane
package Physics
import Graph

// extension method on Graph$Node
// this grants : call syntax but not access to _id
Float kineticEnergy(this Graph$Node, velocity Float) {
    return this.scale * velocity * velocity * Float(0.5)
    // this._id would be a compile error: _id is private to Graph
}
```

```zane
package Main
import Graph
import Physics

Void main() {
    node Graph$Node(Int(1))
    energy Float = node:kineticEnergy(Float(3))

    // referenced as a value via Physics namespace
    fn (Graph$Node, Float) -> Float = Physics$kineticEnergy
}
```

---

## 9. Packages and the Instanceful Package Pattern

### 9.1 Packages are namespaces

A `package X` is a namespace containing type declarations, immutable constants, and static free functions. Namespace members are accessed via `X$member`.

### 9.2 A package may define a class of the same name

If `package Math` defines `class Math`, the package provides both static utilities (via `Math$`) and an instantiable stateful object (via construction).

```zane
package Math
import Log

pi Float(3.141592)

Float radsToDeg(x Float) {
    return x / pi * Float(180)
}

class Math {
    log Log
}

Math(log Log) {
    return init{
        log: log
    }
}

Void debugPi(this Math) mut {
    this.log:write(Math$radsToDeg(pi))
}
```

```zane
package Main
import Math
import Log

Void main() {
    log Log("stdout")

    // static function — no instance needed
    deg Float = Math$radsToDeg(Float(1))

    // instanceful class
    math Math(log)
    math:debugPi()
}
```

### 9.3 Scope isolation

Constructor and method bodies cannot access package-level values directly. Any package-level state a constructor or method needs must be passed as an explicit parameter. This keeps construction deterministic and prevents hidden dependencies on package state.

```zane
package Graph

counter Int = Int(0)

class Node {
    _id Int
    scale Float
}

// compile error if counter is referenced inside init{ } or the body
Node(id Int) {
    return init{
        _id: id,
        scale: Float(1)
        // counter not accessible here
    }
}

// correct: pass counter explicitly
Node(id Int, count Int) {
    return init{
        _id: id,
        scale: Float(count)
    }
}
```

---

## 10. Connection to the Effect Model

Every function and method is **read-only by default**. Reading `this.field` in a method is semantically equivalent to reading a parameter in a free function — both are reading an input. The only way to mutate state is through a `mut` method on a receiver.

Because Zane enforces single ownership, the compiler knows statically which parts of the object graph a `mut` call can affect:

- Two `mut` calls on **different** instances can be safely parallelized.
- Two `mut` calls on the **same** instance must be serialized.
- Read-only calls on the same instance can be reordered freely if no `mut` call intervenes.

> **See also:** [`purity.md`](purity.md) for the complete effect model, inferred purity levels, capability wiring, and concurrency implications.

---

## 11. Summary

| Concept | Rule |
|---|---|
| Class body | Fields only — no methods, no constructors |
| Struct | Value type; fields only; no class or ref fields |
| Constructor | Package-scope declaration; no `this`; returns `init{ }` |
| `init{ }` | Raw field injection; constructor body only; all fields required |
| Method | Free function with first param named `this` |
| Free function | First param not named `this`; no private field access |
| `mut` | Modifier on method; grants write access to `this`; not an overload discriminator |
| Parameters | Always read-only; never mutated directly |
| Private fields | `_`-prefixed; accessible only via `this` in defining package |
| Extension | Any package may define methods on imported types; public fields only |
| Overload identity | Parameter types only; names, `mut`, and return type are not discriminators |
| Method as value | Referenced via `Package$functionName`; `this` becomes explicit first arg |
| Bound references | Not built-in; wrap in a lambda explicitly |
| Package | Namespace; same-named class enables instanceful pattern |
| Scope isolation | Constructor/method bodies cannot access package-level values directly |
