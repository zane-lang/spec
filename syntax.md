# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. The individual spec documents ([`oop.md`](oop.md), [`purity.md`](purity.md), [`error_handling.md`](error_handling.md), [`memory_model.md`](memory_model.md), [`dependency_management.md`](dependency_management.md)) describe the *semantics* of each construct. This document describes the *form* — how to write them.

---

## 1. Declarations

### 1.1 Variables

```
name Type = expr          // owning variable
name ref Type = expr      // non-owning reference
```

The type annotation may be omitted when it is unambiguously inferred from the right-hand side:

```zane
x Int = Int(5)
y = Int(5)        // type inferred as Int
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
(ParamType, ParamType, ...) -> ReturnType
(ParamType, ...) -> ReturnType ? AbortType
(mut ReceiverType, ParamType, ...) -> ReturnType
(mut ReceiverType, ParamType, ...) -> ReturnType ? AbortType
```

`mut` as the first parameter modifier indicates that calling through this function value will mutate the receiver.

Examples:
```
(Int) -> Int                            // takes Int, returns Int, cannot abort
(String) -> Int ? String                // takes String, returns Int, aborts with String
(mut FileSystem, String) -> Int ? Void  // mut method reference, aborts with Void
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

When used as a value, `this` becomes an explicit first argument in the function type:

```zane
Graph$scaledId    // type: (Graph$Node, Int) -> Int
Graph$setScale    // type: (mut Graph$Node, Float) -> Void
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

// Function type
(ParamTypes) [mut] -> ReturnType ? AbortType

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
