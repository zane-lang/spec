# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. The individual spec documents ([`oop.md`](oop.md), [`purity.md`](purity.md), [`error_handling.md`](error_handling.md), [`memory_model.md`](memory_model.md), [`dependency_management.md`](dependency_management.md)) describe the *semantics* of each construct. This document describes the *form* — how to write them.

---

## 1. Declarations

### 1.1 Symbols (variables)

New symbol — type only, no initial value:
```
name Type
name ref Type
```

New symbol with an initial value, using constructor syntax inline:
```
name Type(arg, ...)           // positional constructor
name Type{key: val, ...}      // named constructor
```

Assigning or overwriting an existing symbol — the type is not required; any expression that produces the right type is valid:
```
name = expr           // any expression: function call, constructor, field access, etc.
name = Type(...)      // positional constructor
name = Type{...}      // named constructor
```

```zane
hp Int                          // declared, no value yet
hp Int(100)                     // declared and initialized
hp = Int(50)                    // overwritten with constructor
hp = computeHp()                // overwritten with function result

vec Vec2                        // declared
vec = Vec2(Int(0), Int(15))     // constructor call
vec = getRandomVec()            // function returning Vec2

myTank ref Tank                 // non-owning reference, uninitialized
myTank ref Tank = tanks[0]      // non-owning reference to an existing object
```

### 1.2 Constants (package-level)

```
name Type(value)
```

```zane
package Math

pi Float(3.141592)
```

### 1.3 Class fields

```zane
class Name {
    fieldName Type          // owned field
    fieldName ref Type      // non-owning reference field
}
```

Fields beginning with `_` are private (accessible only via `this` in the defining package). All other fields are public.

### 1.4 Struct fields

```zane
struct Name {
    fieldName Type    // primitives and other structs only — no class or ref fields
}
```

### 1.5 Imports

```zane
import aliasKey
```

The alias must be a key in the project's `zane.coda` manifest. Package members are then accessed as `aliasKey$member`.

---

## 2. Types

### 2.1 Primitive types

`Int`, `Float`, `Bool`, `String`, `Void`

### 2.2 Class and struct types

```
PackageName$ClassName       // fully qualified
ClassName                   // within the same package
```

### 2.3 Reference types

```
ref Type                    // non-owning reference to a heap object
```

```
List<Type>                  // list that owns its elements
List<ref Type>              // list of non-owning references
```

### 2.4 Function types

```
(ParamType, ...) -> ReturnType                                     // free function
(ParamType, ...) -> ReturnType ? AbortType                         // abortable free function
(this ReceiverType, ParamType, ...) -> ReturnType                  // read-only method
(this ReceiverType, ParamType, ...) mut -> ReturnType              // mutating method
(this ReceiverType, ParamType, ...) -> ReturnType ? AbortType      // abortable method
(this ReceiverType, ParamType, ...) mut -> ReturnType ? AbortType  // mutating abortable method
```

`mut` is placed after the closing `)` of the parameter list, before `->`. It is **only valid when the first parameter is named `this`**. A function type with `mut` but no `this` first parameter is a compile-time error — `this` is what designates what value may be mutated; without it, `mut` has no meaning.

```
(this FileSystem, String) -> Int ? Void   // read-only method, aborts with Void
(this FileSystem, String) mut -> Void     // mutating method, cannot abort
(Int) -> Int                              // free function — no mut allowed
(Node, Int) mut -> Int                    // ILLEGAL: mut without this
```

Free functions and static functions cannot mutate any of their parameters. Mutation is expressed exclusively through `mut` methods on a receiver. `this` in the function type is the explicit declaration of what may be mutated.

`this` is a reserved parameter name. It may only appear as the **first** parameter; using `this` in any other position is a compile-time error.

```
(Int, this Node) -> Void    // ILLEGAL: this must be the first parameter
```

---

## 3. Functions, Methods, and Constructors

### 3.1 Free function

```
ReturnType name(param Type, ...) { body }
ReturnType name(param Type, ...) => expr
```

### 3.2 Abortable free function

```
ReturnType ? AbortType name(param Type, ...) { body }
ReturnType ? AbortType name(param Type, ...) => expr
```

### 3.3 Method (first parameter named `this`)

```
ReturnType name(this ReceiverType, param Type, ...) { body }
ReturnType name(this ReceiverType, param Type, ...) => expr
```

`this` must be the first parameter. It is a compile-time error to use `this` as the name of any parameter other than the first.

### 3.4 Mutating method

```
ReturnType name(this ReceiverType, param Type, ...) mut { body }
```

### 3.5 Abortable method

```
ReturnType ? AbortType name(this ReceiverType, param Type, ...) [mut] { body }
```

### 3.6 Positional constructor

```
TypeName(param Type, ...) { return init{ fieldName: expr, ... } }
```

Invoked as `Package$TypeName(args)` or `TypeName(args)` within the same package.

### 3.7 Named constructor

```
TypeName {
    paramName Type,
    paramName Type(defaultExpr),
    ...
} {
    return init{ fieldName: expr, ... }
}
```

Invoked as `Package$TypeName{ paramName: expr, ... }`.

### 3.8 `init{ }` — constructor body only

```
return init{
    fieldName: expr,
    ...
}
```

All fields of the type must be present. `init{ }` is only valid as a return expression inside a constructor body.

---

## 4. Call Syntax

### 4.1 Constructor call

```zane
// Positional
n Graph$Node(Int(1))
n Graph$Node(Int(1), Float(2))

// Named
n Graph$Node{ id: Int(1) }
n Graph$Node{ id: Int(1), scale: Float(2) }
```

Positional and named arguments cannot be mixed in a single call.

### 4.2 Method call (`:`)

```zane
receiver:methodName(arg, ...)
receiver:methodName()
```

`:` is only valid for functions whose first parameter is named `this`. It is call syntax only — it cannot produce a reference or a value.

### 4.3 Free function call

```zane
FunctionName(arg, ...)
Package$FunctionName(arg, ...)
```

### 4.4 Function reference (value)

```zane
Package$functionName          // method or free function as a first-class value
```

When used as a value, `this` appears explicitly as the first parameter in the function type, and `mut` appears after the parameter list if the method is mutating:

```zane
Graph$scaledId    // type: (this Graph$Node, Int) -> Int
Graph$setScale    // type: (this Graph$Node, Float) mut -> Void
Graph$getScale    // type: (Graph$Node) -> Float
```

### 4.5 Package member access

```zane
Package$memberName    // constant, type, or function reference
```

---

## 5. Error Handling

### 5.1 `?` handler block

```
expr ? binder { handlerBody }     // AbortType is a named type
expr ? { handlerBody }            // AbortType is Void
```

Every path through `handlerBody` must end with one of:

```
resolve expr      // substitute a value; execution continues after the call expression
return expr       // exit the enclosing function via its primary return path
abort expr        // exit the enclosing function via its abort path
abort             // exit via abort path (AbortType is Void)
```

### 5.2 `??` coalescing shorthand

```
expr ?? defaultExpr
```

Equivalent to `expr ? _ { resolve defaultExpr }`. Valid for any abort type including `Void`.

### 5.3 Summary

```
// Declaration
ReturnType ? AbortType name(params) [mut] { body }
ReturnType ? AbortType name(params) [mut] => expr

// Function type (free function — no mut)
(ParamTypes) -> ReturnType ? AbortType

// Function type (method — mut only valid with this as first param)
(this ReceiverType, ParamTypes) [mut] -> ReturnType ? AbortType

// Handler block
expr ? binder { ... resolve Value ... }
expr ? { ... resolve Value ... }        // Void abort type

// Coalescing
expr ?? DefaultValue

// Inside a handler block
resolve Value     // substitute success value; continue parent function
return Value      // exit parent function via primary path
abort Value       // exit parent function via secondary path
abort             // exit parent function via secondary path (Void abort type)
```

---

## 6. Ownership and Refs in Declarations

```zane
// Owning variable — controls the object's lifetime
tank Tank = Tank(...)

// Non-owning reference — does not control lifetime
myTank ref Tank = tanks[0]

// Owning list — list owns all elements
tanks List<Tank> = List<Tank>()

// Non-owning reference list — list holds refs, elements owned elsewhere
visible List<ref Tank> = List<ref Tank>()

// Class with owning and non-owning fields
class World {
    player Player           // World owns this Player
    spectated ref Player    // non-owning reference to a Player owned elsewhere
    tanks List<Tank>        // World owns the list and all Tanks
    visible List<ref Tank>  // World holds refs to Tanks owned elsewhere
}
```

---

## 7. Packages

```zane
package Name              // declare package membership
import aliasKey           // import a dependency by manifest key

Name$member               // access a package-level member
```

A package may define a class of the same name to enable the instanceful pattern:

```zane
package Math

pi Float(3.141592)        // package-level constant

class Math {              // instanceful: Math can also be constructed
    log Log
}

Math(log Log) {           // constructor for the Math class
    return init{ log: log }
}

Void debugPi(this Math) mut {  // method on the Math class
    this.log:write(Math$radsToDeg(pi))
}
```
