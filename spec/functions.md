# Zane Functions, Methods, and Lambdas

This document specifies Zane's function model: functions, methods, subscripts, overload resolution, function values, lambdas, and method name resolution. Data declarations and constructors live in [`types.md`](types.md); the package-scope rules that host these declarations live in [`packages.md`](packages.md).

> **See also:** [`types.md`](types.md) §3 for constructors. [`memory.md`](memory.md) §2 for ownership and `&` rules. [`effects.md`](effects.md) §2 for `mut`. [`syntax.md`](syntax.md) §3 for declaration grammar.

---

## 1. Overview

Zane unifies methods, functions, and lambdas under one model: a callable is a package-scope declaration (or anonymous literal) whose first parameter may optionally be `this`.

- **`Verb`.** A **verb** is a callable whose body is a sequence of statements that executes to do work: functions, methods, operators, constructors, and lambdas (a lambda being an anonymous verb). The spec uses "verb" whenever a rule applies to all of these as a group, and reserves "function" for the narrow form — an ordinary identifier-named verb with no `this`. A subscript is not a verb — its body must be a place expression that projects a place rather than running computation (§2.9).
- **`Package-scope behavior`.** All methods, functions, and constructors are declared at package scope; type bodies never contain behavior.
- **`Methods as verbs`.** A method is a verb whose first parameter is `this`, so methods and functions share one model and differ only by the receiver.
- **`Explicit mutation at the call site`.** `:` calls are read-only; `!` calls invoke `mut` methods.
- **`Overload identity is parameter types only`.** Names, return type, and `mut` do not distinguish overloads.

---

## 2. Methods

### 2.1 Methods are verbs whose first parameter is `this`
A method is any package-scope verb whose first parameter is named `this`. `this` **MUST** be the first parameter and **MUST NOT** appear in any other parameter position.

```zane
Int scaledId(this Node, factor Int) {
    return this._id * factor
}
```

### 2.2 `this` grants private-field access
Naming the first parameter `this` is the only thing that makes a declaration a method. That token grants access to `_`-prefixed fields on the receiver type regardless of which package declares the method; home-package status does not matter. The same parameter type written with another name is a function and does not grant private-field access.

```zane
Int scaledId(this Node, factor Int) {
    return this._id * factor
}

Int scaledIdWrong(node Node, factor Int) {
    return node._id * factor   // ILLEGAL: node is not `this`
}
```

### 2.3 Read-only methods are the default
A method without `mut` may read `this`, its parameters, and reachable read-only state, but it may not write to `this` or owned descendants.

### 2.4 Mutating methods use `mut`
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

### 2.5 Call markers are part of the surface syntax
Read-only methods are called with `:`. Mutating methods are called with `!`.

```zane
node:scaledId(Int(2))
node!setScale(Float(3))
```

Calling a `mut` method with `:` is illegal. Calling a non-`mut` method with `!` is also illegal.

### 2.6 Method desugaring

```zane
receiver:method(arg)        → ResolvedPkg$method(receiver, arg)
receiver!method(arg)        → ResolvedPkg$method(receiver, arg)
receiver:Pkg$method(arg)    → Pkg$method(receiver, arg)
receiver!Pkg$method(arg)    → Pkg$method(receiver, arg)
```

### 2.7 Parameters are read-only
Explicit parameters other than `this` are read-only. Mutation of another object must be expressed as a `mut` method call on that object as the receiver.

### 2.8 `&` method parameters
A method parameter declared as `&T` requires the caller to supply a source that may create a new `&` under [`memory.md`](memory.md) §2.8 and permits the callee to store that argument into an `&` field. A parameter declared as plain `T` is value-only. A plain `T` parameter does not guarantee a stable `&`-rootable source location, therefore it **MUST NOT** be bound into `&` storage.

```zane
type Car = class {
    engine &Engine;
    _value Int;
}

// legal: engine may be stored into an `&` field
Void consume(this Car, engine &Engine) mut {
    this.engine = engine   // legal
}

// legal: engine is value-only; it may only be read
Int calculate(this Car, engine Engine) {
    return this._value + engine.speed   // legal: reading only
}

// ILLEGAL: plain parameter stored into `&` field
Void consumeWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL
}
```

Call syntax is uniform regardless of whether a parameter is `&`:

```zane
engine Engine()
car!consume(engine)        // legal: engine may create a new `&`
car:calculate(engine)      // legal: plain `T` accepts the same argument
car!consume(Engine())      // ILLEGAL: temporary cannot bind to `&` parameter
car:calculate(Engine())    // legal: plain T parameter accepts a temporary
```

### 2.9 Subscripts are place projections
Subscripts are package-scope declarations with the receiver first:

```zane
(this CustomList)[index Int] => this._data[index]
(this Tensor3)[x Int, y Int, z Int] => this._data[x][y][z]
```

The body of a subscript definition **MUST** be a place expression. `[]` is not a general function call and cannot return a computed value. Its result is always inferred from the projected place, so subscripts have no explicit return type annotation. A subscript may declare any number of comma-separated parameters inside `[]`; it is not limited to one or two.

When a receiver interprets an `Int` subscript as an ordinal position in an ordered sequence, that position is 1-based. The first element is at `1`, and a sequence with `n` elements uses `1` through `n` as its positional range.

> **See also:** [`memory.md`](memory.md) §2.8 for when a place expression may create a new `&`.

```zane
(this CustomList)[index Int] => this._data.compute(index)   // ILLEGAL: body is not a place expression
Int (this CustomList)[index Int] => this._data[index]       // ILLEGAL: explicit return type not allowed
```

`list[i]` is a place expression only if `list` is a place expression. `CustomList()[1]` is therefore not a place expression because the base is a temporary.

---

## 3. Functions

### 3.1 Functions are package-scope verbs without `this`
A function is an ordinary identifier-named package-scope verb whose first parameter is not named `this`. (Operators are symbol-named and constructors are named after their type, so neither is a function even though they also take no `this`.)

```zane
Float getScale(node Node) {
    return node.scale
}
```

### 3.2 Functions cannot access private fields
Functions may access only fields whose names do not begin with `_`. This rule is package-independent: a function declared in the same package as the type still cannot access `_`-prefixed fields unless its first parameter is named `this`.

### 3.3 Functions use ordinary call syntax
Functions are called as `name(args...)` or `packageName$name(args...)`.

### 3.4 Expression-bodied verbs
A verb that returns a value may use `=>` for its body. Functions, methods, operators, constructors, and lambdas all support this shorthand (operators are covered in [`operators.md`](operators.md), constructors in [`types.md`](types.md) §3.2):

```zane
Int double(value Int) => value * 2
Int scaledId(this Node, factor Int) => this._id * factor
```

`=> expr` is **purely a surface shorthand**: it means exactly `{ return expr }` and adds no other behavior. A constructor's `=> init{...}` is the same rewrite — `Vec2(x Float, y Float) => init{x, y}` is shorthand for `{ return init{x, y} }`. Because the shorthand always returns its expression, it is illegal for declarations whose return type is `Void`.

---

## 4. Overloading Rules

### 4.1 Overload identity is parameter types only
Two declarations in the same package conflict when they have the same ordered parameter types. Parameter names, `this`, `mut`, and return type do not distinguish overloads.

Two overloads **MUST NOT** differ only by whether the same parameter position is `T` versus `&T`. Such declarations are illegal and the compiler **MUST** reject them with a compile-time error, for example: "illegal overload set: differs only by `&` on a parameter; rename one declaration or choose a single signature."

```zane
Void consume(this Car, engine Engine)
Void consume(this Car, engine &Engine)  // ERROR
```

### 4.2 Consequences of the overload identity rules
Declarations that differ only by return type, parameter names, `this`, or `mut` are compile-time conflicts.

### 4.3 Valid overloads differ by arity or parameter type
Legal overload sets must differ in the number of parameters or in at least one parameter type other than bare `&`-ness at the same position.

### 4.4 Variant case overloads
A function may be overloaded on the individual cases of a `variant` to express pattern matching. These case overloads are subject to two extra restrictions: a whole-variant overload `f(x V)` and a case overload `f(x V.case)` for the same variant **MUST NOT** coexist for one function name, because a call on a whole-variant value could not tell which to select; and all of a function's case overloads over one variant **MUST** be declared in the same package, where the compiler verifies they cover every case. The full rules live in [`adt.md`](adt.md) §5.

> **See also:** [`adt.md`](adt.md) §5 for case-overload dispatch and the whole-variant-versus-case mutual-exclusion rule.

---

## 5. Overload Resolution with Implicit Constructors

For function calls, constructor calls, and desugared method calls, overload resolution proceeds in three phases:

1. **Direct match.** A candidate is viable only if the call type-checks with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
2. **Generic match.** If the direct phase finds no viable candidate, the called declaration's `<>` header parameters are inferred from the static types of the call arguments: a type parameter from an argument's type, a number parameter from the number part of an argument's type. A call never carries a `<>` type-argument list; a type or number may instead be passed as an ordinary argument to a `Type` or `Number` value parameter (see [`generics.md`](generics.md) §5). Inference proceeds under the rules of [`generics.md`](generics.md) §3 and §5, still with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
3. **Implicit match.** If the direct and generic phases find no viable candidate, implicit constructors may be inserted at coercion sites. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.

If no phase yields a viable candidate, the call is a normal no-match type error.

Implicit constructors are therefore a last resort. They never participate in discovering an otherwise unknown destination type for generic inference.

These phases describe **static** overload resolution. Dispatch over a `variant`'s cases is a separate, **runtime** mechanism: a value whose static type is a whole variant lowers to a tag jump over the matching case overloads, rather than being resolved statically. See [`adt.md`](adt.md) §5.

> **See also:** [`types.md`](types.md) §4 for the definition and constraints of implicit constructors. [`adt.md`](adt.md) §5 for runtime case-overload dispatch.

---

## 6. Method Name Resolution and Extension Methods

### 6.1 Unqualified method lookup
For `receiver:methodName(...)` or `receiver!methodName(...)`, the compiler resolves candidates in this order:

1. the receiver type's home package
2. the current package

If no candidate matches, the call is a compile-time error. If multiple candidates remain after overload resolution, the call is a compile-time error and must be written with an explicit package qualifier.

### 6.2 Qualified method calls
Cross-package extension methods are written explicitly:

```zane
vec:Physics$kineticEnergy()
```

### 6.3 Extension methods may be declared in any package
Because methods are package-scope verbs, any package may define methods on imported types. This follows the same rule as [`types.md`](types.md) §2.3 and §2.2 above: if the first parameter is `this`, the declaration is a method and gets the same private-field access as any other method on that receiver type.

---

## 7. Function Values and Lambdas

### 7.1 Callables cannot be referenced as values
Methods, functions, and operators are **call-only**. A package-scope callable name may appear only in call position; there is no syntax that turns it into a value. This is exactly the rule that already governs operators: `+` can be called, but `+` cannot be written as a value.

```zane
Graph$scaledId(node, Int(2))   // legal: call position
Graph$scaledId                 // ILLEGAL: callables cannot be referenced as values
```

The reason is the same one that makes operators safe to overload. An overloaded name is not a single value; it is a set of candidates that only collapses to one when arguments are supplied at a call site. In value position there are no arguments, so nothing selects a member of the set. Passing an overloaded callable into an overloaded parameter would have to choose on both sides at once with no information to choose from. Making callables call-only removes that whole class of ambiguity by construction.

> **See also:** [`operators.md`](operators.md) §5.3 for the parallel rule on operators.

### 7.2 Lambdas are self-typed function values
A lambda literal is a function declaration with the name removed. It writes its own parameter types, return type, abort type, and `mut` (see [`syntax.md`](syntax.md) §3.8). Nothing is inferred from context.

```zane
receiver(Float(x Int) {
    if x < Int(10) {
        return Float(0)
    } else {
        return Float(1) / Float(x)
    }
})
```

Because a lambda carries its complete type, it is a single value with one exact type. It can therefore be passed to an **overloaded** receiver without ambiguity: the lambda fixes its own type, so overload resolution on the receiver proceeds with ordinary argument types and no circularity. Self-typed lambdas can also be written and passed directly in the same expression, since they no longer depend on a surrounding context to fix their type.

`mut` is part of the lambda's written type. A lambda that does not declare `mut` may still be assigned to a `mut` function type — it simply does not use the mutation permission — but a `mut` lambda may not be assigned to a non-`mut` function type:

```zane
onEventCallback Void[this Node, EventData] mut = Void(this Node, data EventData) {
    ...
} // OK: non-`mut` lambda assigned to a `mut` function type

readonlyCallback Void[this Node, EventData] = Void(this Node, data EventData) mut {
    ...
} // ILLEGAL: expected a non-`mut` function value
```

### 7.3 Lambda-variables hold function values
To name a function value, declare a **lambda-variable**: a symbol bound to a lambda literal. The shorthand mirrors constructor-call instantiation, swapping the name and return type relative to a function declaration:

```zane
Float callback(x Int) { ... }   // function declaration
callback Float(x Int) { ... }   // lambda-variable declaration
```

A lambda-variable is an ordinary symbol with a single function type. Because a symbol cannot be redeclared with a different type, a lambda-variable name can never accumulate an overload set, so it is always unambiguous in value position. This is what makes `receiver(callback)` well-defined where referencing an overloaded callable would not be.

> **See also:** [`syntax.md`](syntax.md) §2.9 for function types and §3.8 for lambda literals and lambda-variable declarations.

### 7.4 Lambdas do not capture
Lambdas **MUST NOT** capture outer variables. Every dependency must be passed as a parameter or supplied through surrounding storage explicitly. See [`concurrency.md`](concurrency.md) §5.2 ("Lambdas do not capture").

### 7.5 No bound method references
Zane does not provide bound method references as a separate feature. Because lambdas do not capture, there is no syntax that implicitly stores a receiver inside a function value. Code that needs a receiver later must keep that receiver in ordinary storage and pass it explicitly when the function value is invoked.

---

## 8. Connection to the Effect Model

Read-only methods and functions are effect-free with respect to their receiver unless they touch refs or capabilities. `mut` marks the only direct path for writing receiver-owned state. This is why overload identity ignores `mut`: the call contract is structurally the same even though the behavioral permissions differ.

> **See also:** [`effects.md`](effects.md) for the complete effect model and concurrency implications.

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| Methods are verbs with `this` | Keeps the language model flat: methods are ordinary verbs with one extra permission token. |
| `&` parameters in constructors and methods | An `&` field must be initialized from an allowed `&` source; requiring `&` on the corresponding parameter makes this constraint visible in the signature without ghost refs or hidden storage creation. |
| Plain `T` parameters are value-only | A caller is not required to supply a stable storage location for a plain parameter; restricting plain parameters from populating `&` fields prevents hidden dependency on call-site expression form. |
| `:` and `!` are distinct call markers | Makes mutation visible at the call site without adding mutable-reference types. |
| Subscripts are place projections only | Keeps `[]` predictable: an indexed expression always projects existing storage rather than running arbitrary computation. |
| No overloads that differ only by `&` | Call syntax stays uniform while avoiding overload ambiguity between value-only and place-required contracts. |
| Overload identity ignores `mut` | The call contract (parameter types and arity) is the same; `mut` only adjusts the permission granted to `this`, so it cannot disambiguate two otherwise-identical declarations. |
| Overload resolution phases: direct, generic, implicit | Makes implicit conversions a fallback after exact matches, preventing surprising behavior when an exact match exists. |
| Generic match infers type parameters | A call carries no `<>` list, so the generic-match phase is purely an inference step driven by the argument types (see [`generics.md`](generics.md) §5). |
| One pass handles types and numbers | Both type parameters and number parameters of the called declaration are inferred from the argument types in the same pass, since the two kinds share one header form (see [`generics.md`](generics.md) §3). |
| Callables are call-only | An overloaded name is a candidate set, not a value; it only collapses when arguments are supplied at a call site. Keeping methods, functions, and operators call-only removes the ambiguity of passing an overloaded name into an overloaded parameter, exactly as operators already avoid it. |
| Self-typed lambdas | A lambda carries its own complete type, so it is a single value that overload resolution can match without circularity even when the receiver is overloaded. |
| Lambda-variables for function values | A named lambda-variable has one function type and cannot accumulate an overload set, so it is always unambiguous in value position. |
| No lambda capture | Preserves explicit data flow and keeps effect analysis tractable. |
| Home-package-first method lookup | Makes unqualified method calls locally understandable and unaffected by imports. |

---

## 10. Summary

| Concept | Rule |
|---|---|
| Method | Package-scope verb whose first parameter is `this` |
| `mut` method | Called with `!`; receiver MUST be a class; may mutate `this` and its owned subtree |
| Read-only method | Called with `:`; may read but not write `this` |
| Function | Package-scope verb without `this`; no private-field privilege |
| `&` method parameter | Caller must supply an allowed `&` source; callee may store into `&` fields |
| Plain `T` method parameter | Value-only; caller may supply a temporary; callee **MUST NOT** bind it into `&` storage |
| Subscript | Package-scope place projection written `(this T)[...] => placeExpr`; no explicit return type |
| Overload identity | Parameter types only; not names, return type, or `mut`; overloads differing only by `&` at one position are illegal |
| Overload resolution phases | Direct match, then generic match, then implicit match; ambiguity within any one phase is an error |
| Callable reference | Illegal; methods, functions, and operators are call-only and have no value form |
| Lambda | Self-typed function value: explicit parameter types, return type, abort type, and `mut`; no capture |
| Lambda-variable | Symbol bound to a lambda literal; has one function type; the only way to hold a function value |
| Unqualified method lookup | Searches home package, then current package |
| Extension methods | Any package may declare methods on imported types by naming the first parameter `this` |
