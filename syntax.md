# Zane Syntax Reference

This document is the canonical reference for Zane's surface syntax. Topic documents describe semantics; this document describes form only.

> **See also:** [`oop.md`](oop.md) for object declarations. [`error_handling.md`](error_handling.md) for abort semantics. [`operators.md`](operators.md) for precedence rules.

---

## 1. Declarations

### 1.1 Symbols (variables)
New symbol declarations:

```
name Type
name ref Type
name Type(args, ...)
name Type{key: val, ...}
name Type = expr
name ref Type = expr
```

Once a symbol exists, assignment uses:

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
Type         // within the same package
```

### 2.3 Reference types (storage only)

```
ref Type
```

`ref` appears only in storage positions (variables, fields, container element types).

### 2.4 Type parameters

```
struct Box<T> { ... }
```

### 2.5 Const parameters and identifiers

Definition-site binders:

```
struct Matrix[rows]X[cols]<T> { ... }
```

Use-site forms:

```
Matrix10X20<Float>
```

Digits are illegal in identifiers unless they represent const arguments in a const-parameterized type name.

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

`mut` is only valid when the first parameter is `this`.

---

## 3. Functions and Methods

### 3.1 Free function declarations

```
ReturnType name(param Type, ...)
ReturnType ? AbortType name(param Type, ...)
```

### 3.2 Method declarations

```
ReturnType name(this Type, param Type, ...)
ReturnType name(this Type, param Type, ...) mut
ReturnType ? AbortType name(this Type, param Type, ...) mut
```

### 3.3 `mut` placement
`mut` appears after the parameter list and before the return arrow in function types.

---

## 4. Calls and Concurrency

### 4.1 Function calls

```
Package$fn(args...)
```

### 4.2 Method calls

```
receiver:method(args...)
receiver!method(args...)
receiver:Package$method(args...)
receiver!Package$method(args...)
```

### 4.3 Pipe to last argument

```
callExpr|{ block }
```

### 4.4 `spawn`

```
spawn Package$fn(args...)
name Type = spawn Package$fn(args...)
```

`spawn` is legal only on function call expressions.

### 4.5 No indexing operator
`x[i]` is not valid syntax. Element access uses explicit methods (e.g., `list:at(i)`).

---

## 5. Operators and Keywords

### 5.1 Operators
Tokens: `~`, `*`, `/`, `+`, `-`, `==`, `~=`

### 5.2 Boolean keywords
`and`, `or`

---

## 6. Error Handling Forms

### 6.1 Abortable returns

```
ReturnType ? AbortType
```

### 6.2 `?` handler block

```
callExpr ? err { ... }
callExpr ? { ... }       // when AbortType is Void
```

### 6.3 `??` shorthand

```
callExpr ?? fallbackExpr
```

---
