# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. Topic documents define semantics; this document defines form only.

> **See also:** [`oop.md`](oop.md) for constructors and methods. [`error_handling.md`](error_handling.md) for abort semantics. [`operators.md`](operators.md) for precedence.

---

## 1. Declarations

### 1.1 Symbols

New symbol declarations:

```
name Type
name ref Type
name Type(args, ...)
name Type{field: expr, ...}
name Type{fieldA, fieldB, ...}
name Type = expr
name ref Type = expr
```

`Type{fieldA, fieldB}` is shorthand for `Type{fieldA: fieldA, fieldB: fieldB}`.

Once a symbol already exists, reassignment uses only:

```
name = expr
```

### 1.2 Package constants

```
name Type(value)
```

### 1.3 Class fields

```
class Name {
    field Type
    field ref Type
}
```

### 1.4 Struct fields

```
struct Name {
    field Type
}
```

### 1.5 Imports

```
import aliasKey
```

---

## 2. Types

### 2.1 Primitive types
`Int`, `Float`, `Bool`, `String`, `Void`

### 2.2 Named types

```
Package$Type
Type
```

### 2.3 Reference types

```
ref Type
```

`ref` is valid only in storage positions.

### 2.4 Type parameters

```
struct Box<T> { ... }
```

### 2.5 Const-parameterized types

Definition-site binders:

```
struct Matrix[rows]X[cols]<T> { ... }
```

Use-site form:

```
Matrix10X20<Float>
```

Digits are illegal in identifiers except where they supply const arguments to a const-parameterized type name.

### 2.6 Array primitive

```
Array[size]<T>
```

### 2.7 Function types

```
(ParamType, ...) -> ReturnType
(ParamType, ...) -> ReturnType ? AbortType
(this ReceiverType, ParamType, ...) -> ReturnType
(this ReceiverType, ParamType, ...) mut -> ReturnType
(this ReceiverType, ParamType, ...) -> ReturnType ? AbortType
(this ReceiverType, ParamType, ...) mut -> ReturnType ? AbortType
```

`mut` is legal only when the first parameter is `this`.

---

## 3. Functions, Methods, Constructors, and Lambdas

### 3.1 Free functions

```
ReturnType name(param Type, ...) { body }
ReturnType ? AbortType name(param Type, ...) { body }
```

### 3.2 Methods

```
ReturnType name(this ReceiverType, param Type, ...) { body }
ReturnType name(this ReceiverType, param Type, ...) mut { body }
ReturnType ? AbortType name(this ReceiverType, param Type, ...) { body }
ReturnType ? AbortType name(this ReceiverType, param Type, ...) mut { body }
```

### 3.3 Positional constructors

```
TypeName(param Type, ...) {
    return init{ field: expr, ... }
}
```

### 3.4 Field constructors

```
TypeName{
    fieldA Type,
    fieldB Type,
    ...
} {
    return init{fieldA, fieldB}
}
```

Field-constructor call sites may use explicit or implicit field names:

```
name TypeName{fieldA: expr, fieldB: expr}
name TypeName{fieldA, fieldB}
```

### 3.5 `init{ }`

```
return init{
    field: expr,
    otherField,
    ...
}
```

A bare field name inside `init{ }` is shorthand for `fieldName: fieldName`.

### 3.6 Lambda declarations

```
(ParamType, ...) -> ReturnType name = (paramName Type, ...) {
    ...
}

(this ReceiverType, ParamType, ...) mut -> ReturnType name = (this ReceiverType, paramName Type, ...) mut {
    ...
}
```

Example:

```zane
(this Box<Int>, Int) mut -> Void callback = (this Box<Int>, Int) mut {
    ...
}
```

---

## 4. Calls and Function Values

### 4.1 Free-function calls

```
name(args...)
Package$name(args...)
```

### 4.2 Method calls

```
receiver:method(args...)
receiver!method(args...)
receiver:Package$method(args...)
receiver!Package$method(args...)
```

### 4.3 Function references

```
Package$functionName
```

### 4.4 Pipe syntax

```
callExpr|{ block }
```

### 4.5 `spawn`

```
spawn Package$fn(args...)
name Type = spawn Package$fn(args...)
```

`spawn` is legal only on function-call expressions.

### 4.6 No indexing operator
`x[i]` is not valid syntax. Element access is spelled with methods such as `array:at(i)`.

---

## 5. Error Handling

### 5.1 Abortable return types

```
ReturnType ? AbortType
```

### 5.2 `?` handlers

```
expr ? binder { ... }
expr ? { ... }
```

Every path inside the handler must end with one of:

```
resolve expr
resolve
return expr
abort expr
abort
```

### 5.3 `??` shorthand

```
expr ?? fallbackExpr
```

---

## 6. Operators and Keywords

### 6.1 Operators
`~`, `*`, `/`, `+`, `-`, `==`, `~=`

### 6.2 Boolean keywords
`and`, `or`

---

## 7. Packages

### 7.1 Package member syntax

```
Package$member
```

### 7.2 Package declarations

```
package Name
```
