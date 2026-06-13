# Zane Functions, Methods, and Lambdas

This document specifies Zane's function model: free functions, methods, subscripts, overload resolution, function values, lambdas, and method name resolution. Data declarations and constructors live in [`types.md`](types.md); the package-scope rules that host these declarations live in [`packages.md`](packages.md).

> **See also:** [`types.md`](types.md) §3 for constructors. [`memory.md`](memory.md) §2 for ownership and `&` rules. [`effects.md`](effects.md) §2 for `mut`. [`syntax.md`](syntax.md) §3 for declaration grammar.

---

## 1. Overview

Zane unifies methods, free functions, and lambdas under one model: a callable is a package-scope declaration (or anonymous literal) whose first parameter may optionally be `this`.

- **`Package-scope behavior`.** All methods, free functions, and constructors are declared at package scope; type bodies never contain behavior.
- **`Methods as functions`.** A method is a function whose first parameter is `this`, so methods and function values share one model.
- **`Explicit mutation at the call site`.** `:` calls are read-only; `!` calls invoke `mut` methods.
- **`Overload identity is parameter types only`.** Names, return type, and `mut` do not distinguish overloads.

---

## 2. Methods

### 2.1 Methods are functions whose first parameter is `this`
A method is any package-scope function whose first parameter is named `this`. `this` **MUST** be the first parameter and **MUST NOT** appear in any other parameter position.

```zane
Int scaledId(this Node, factor Int) {
    return this._id * factor
}
```

### 2.2 `this` grants private-field access
Naming the first parameter `this` is the only thing that makes a declaration a method. That token grants access to `_`-prefixed fields on the receiver type regardless of which package declares the method; home-package status does not matter. The same parameter type written with another name is a free function and does not grant private-field access.

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
class Car {
    engine &Engine
    _value Int
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

## 3. Free Functions

### 3.1 Free functions are package-scope functions without `this`
A free function is any package-scope function whose first parameter is not named `this`.

```zane
Float getScale(node Node) {
    return node.scale
}
```

### 3.2 Free functions cannot access private fields
Free functions may access only fields whose names do not begin with `_`. This rule is package-independent: a free function declared in the same package as the type still cannot access `_`-prefixed fields unless its first parameter is named `this`.

### 3.3 Free functions use ordinary call syntax
Free functions are called as `name(args...)` or `PackageName$name(args...)`.

### 3.4 Expression-bodied functions and methods
Both free functions and methods may use `=>` when they return a value:

```zane
Int double(value Int) => value * 2
Int scaledId(this Node, factor Int) => this._id * factor
```

`=> expr` desugars to `{ return expr }`. Because the shorthand always returns its expression, it is illegal for declarations whose return type is `Void`.

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

A further consequence is the **no-reference rule**: a function name is not a value, whether it has a single declaration or many. This is the function-value parallel of the operator rule from [`operators.md`](operators.md) §1 ("operators are grammar, not values"). See §4.4 below for the rule and the parallel.

### 4.3 Valid overloads differ by arity or parameter type
Legal overload sets must differ in the number of parameters or in at least one parameter type other than bare `&`-ness at the same position.

### 4.4 Functions are not values in name position
A package-scope function name (a free function, a method, or a constructor) is not a value. There is no surface form that yields a function value from a name. The only way to obtain a function value is a lambda literal (§7) or a lambda variable (§7). The only way to *call* a function is to write the call form: `name(args...)`, `PackageName$name(args...)`, `receiver:method(args...)`, `receiver!method(args...)`, or the qualified-method forms in §2.6.

This rule applies to **every** function name, whether it has a single declaration or multiple overloads. Overloaded names were already not referenceable as values in the older spec; the rule now extends the same treatment to non-overloaded names so the surface is uniform and has no conditional behaviour.

The rule is the function-value parallel of [`operators.md`](operators.md) §1. Both operators and function names are grammar tokens, not values: the only way to obtain an `Int -> Int` callable is either an operator call (`a + b`) or a lambda, never a reference to a function by name. `PackageName$name` is a *call target*, not a value, just as `+` is a fixed grammar token rather than a value the user can name.

```zane
// ILLEGAL: function names are not values
callback Float[Int] = Graph$scale
```

```zane
// ILLEGAL: even non-overloaded free functions are not values
mapper Int[Int] = identity$compose
```

Method desugaring under §2.6 is unaffected. The desugared form `ResolvedPkg$method(receiver, arg)` is internal to the compiler and never appears in user source.

---

## 5. Overload Resolution with Implicit Constructors

For free-function calls, constructor calls, and desugared method calls, overload resolution proceeds in three phases:

1. **Direct match.** A candidate is viable only if the call type-checks with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
2. **Generic match.** If the direct phase finds no viable candidate, type generics are inferred from the *type* part of each call argument's static type, and type-parameter symbols are inferred from the *type-parameter* part. The *type-parameter part* of a static type is the integer value baked into the type identifier (see [`generics.md`](generics.md) §2.4 and §7.1). Type ascriptions on the call are used as a fallback to fix any type generic reachable only through the return type. Inference proceeds under the rules of [`generics.md`](generics.md) §3, §4, and §5, still with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
3. **Implicit match.** If the direct and generic phases find no viable candidate, implicit constructors may be inserted at coercion sites. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.

If no phase yields a viable candidate, the call is a normal no-match type error.

Implicit constructors are therefore a last resort. They never participate in discovering an otherwise unknown destination type for generic inference.

> **See also:** [`types.md`](types.md) §4 for the definition and constraints of implicit constructors.

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
Because methods are package-scope functions, any package may define methods on imported types. This follows the same rule as [`types.md`](types.md) §2.3 and §2.2 above: if the first parameter is `this`, the declaration is a method and gets the same private-field access as any other method on that receiver type.

---

## 7. Lambdas and Lambda Variables

A function value in Zane is always produced by a lambda. A function *name* is never a value (see §4.4). This section defines the two surface forms that produce a function value: a **lambda literal** written inline at a call site or in a typed slot, and a **lambda variable** that names a lambda with a fixed function type. It also defines the function-type ascription that both forms use.

### 7.1 `PackageName$name` is a call target, not a value
A package-qualified function name (`PackageName$name`) is the surface form of a *call*: writing `PackageName$name(args...)` invokes the function. The name is **not** legal in any value position. Every position that expects a function value — the right side of an assignment, an argument to a function or constructor, a typed slot of function type — rejects `PackageName$name` and requires a lambda literal or lambda variable instead.

```zane
// ILLEGAL: function name in value position
callback Float[Int] = Graph$scale

// ILLEGAL: function name passed as an argument
dispatch Int(Int, Float[Int]) = dispatchWith(Graph$scale)

// ILLEGAL: function name in a typed slot
mapper Float[Int] = Graph$scale
```

The rule is the function-value parallel of [`operators.md`](operators.md) §1: operators are a fixed set of grammar tokens, never values, and the same rule now applies uniformly to all function names. See §4.4 for the overload-side statement of the same rule.

### 7.2 Lambda literals
A lambda literal writes a parameter list and a body, optionally with an explicit return type. There are two surface forms.

**Form A — explicit types.** The return type is written first, followed by a parenthesized parameter list in declaration form (name + type per parameter, comma-separated), followed by a body:

```zane
Float(x Int) {
    return Float(x.value) * 2
}
```

This is the explicit-typed literal. The parameter list inside the parentheses is the *lambda body header*, the same form a method or free-function declaration uses for its parameters (see §3.1 and §3.2). For a method lambda, the first parameter is `this`, and `mut` belongs on the function type:

```zane
Void(this Element, data EventData) {
    ...
}
```

```zane
Void mut[this Element, data EventData] {
    ...
}
```

**Form B — contextual typing.** When the surrounding slot already fixes the function type, the return type and parameter types can be omitted:

```zane
element!onClick((eventData) {
    ...
})
```

The contextual form writes only parameter names; the parameter types, return type, and abort type are supplied by the slot. Both forms are always legal; the choice is style. The contextual form is the original Zane lambda literal and is preserved unchanged from the previous spec.

**Both forms compile to the same lambda value.** The following two declarations are equivalent:

```zane
// Form A: explicit return type
callback Float(x Int) {
    return Float(x.value) * 2
}

// Form B: contextual typing (slot supplies Float[Int])
callback Float[Int] = (x) {
    return Float(x.value) * 2
}
```

`mut` is not inferred. A lambda that needs the receiver-mutation contract writes `mut` on the function-type ascription. The contextual form of the lambda literal (the body of an assignment) may also write `mut` between the body header and the body block, and that body-side `mut` must match the mutating contract carried by the ascription side. The two `mut` markers live in different positions and are not interchangeable:

```zane
callback Void mut[this Node, data EventData] = (this, data) {
    ...
} // OK: initial declaration, mutating contract on the ascription

callback = (this, data) mut {
    ...
} // OK: body reassignment, `mut` on the body matches the ascription's mutating contract
```

If the ascription is non-`mut` and the contextual-form body writes `mut`, the body-side marker contradicts the ascription and the assignment is rejected:

```zane
callback Void[this Node, data EventData] = (this, data) mut {
    ...
} // ILLEGAL: ascription is non-`mut`, body-side `mut` does not match
```

Abortable lambda bodies use the same `? AbortType` marker they use in function declarations:

```zane
parserBad String[Int] ? ParseError = (input) {
    ...
    abort ParseError("bad input")
}
```

Because a lambda literal relies on the surrounding slot to fix its type, it cannot be written and called directly in the same expression. The call must go through a name or another surrounding context that already fixes the function type.

### 7.3 Lambda variables
A lambda variable fuses three things: a variable name, a function-type ascription, and a lambda body header. The long form writes them as separate pieces:

```zane
callback Float[Int] = (x) {
    return Float(x.value) * 2
}
```

The shorthand fuses the same three pieces into a single declaration-shaped line:

```zane
callback (Int) -> Float(x Int) {
    return Float(x.value) * 2
}
```

The shorthand parallels the existing constructor shorthand (`text String("hello")` from [`syntax.md`](syntax.md) §1.1): a variable name fused with a callable type fused with a body. Reading the declaration as a lambda declaration gives it the same syntactic role as a free-function or method declaration; reading the type ascription alone recovers the function value's type.

The function-type ascription inside the lambda-variable shorthand uses the **old** `(T) -> R` form on the ascription side. The shorthand is recognized only when a function-type ascription follows the variable name; without it the construct is a function declaration, not a variable declaration. The new bracket form (§7.4) is the standalone function-type notation and is **not** legal in this position.

```zane
// ILLEGAL: type ascription is required by the shorthand
callback (x Int) -> Float {
    return Float(x.value) * 2
}
```

```zane
// ILLEGAL: `mut` is not legal between the ascription and the body header
callback (Int) -> Float(x Int) mut {
    ...
}
```

`mut` belongs on the function-type ascription, not between the ascription and the body header. A lambda that mutates a `this` receiver writes `mut` *inside* the type ascription. The two positions have two canonical forms:

```zane
// Standalone form: ascription is the new bracket form
callback Void mut[this Node, data EventData] = (this, data) {
    ...
}

// Shorthand: ascription is the old (T) -> R form, with `mut` inside the parentheses
callback (this Node) mut -> Void(this Node, data EventData) {
    ...
}
```

Both forms declare the same lambda variable. The standalone form is the assignment shape, where the ascription side may use the new bracket notation; the shorthand fuses the ascription with the body header and is locked to the old ascription form.

**Type-fixity rule.** A lambda variable's type is fixed at the variable's first declaration. Only the body can be reassigned; the type ascription cannot. The new bracket form (§7.4) is the standalone function-type notation that anchors the variable on each reassignment.

```zane
callback Float[Int] = (x) {
    return Float(x.value) * 2
}

callback = (x) {        // legal: reassigns the body, type Float[Int] is preserved
    return Float(x.value) * 3
}

callback Void[] = (x) { // ILLEGAL: type is fixed at first declaration
    ...
}
```

### 7.4 Function-type ascription syntax
A function type is written in *return-type-first* bracket form: the return type, followed by square brackets containing the parameter type list, optionally followed by `mut` and optionally followed by `? AbortType`. The `&` ref-typed parameter and `&` ref-typed return forms use the existing `&` type syntax inside the bracket and as a return-type prefix. The shape is always:

```zane
ReturnType [paramTypes] mut? ?AbortType?
```

The brackets are required, even when the parameter list is empty. A parameterless free function has the type `Void[]`; a one-argument function returning `Float` has the type `Float[Int]`. A method's `this` parameter is the first element of the bracket, and `mut` follows the brackets (it is legal only when the first parameter is `this`):

```zane
// method type
Float[this Player]
Float[this Player, Int] mut
&Float[this Player, &Int] ? AbortType
```

The new bracket form unifies the function type with every other parameterized type in Zane (`Array[size]`, `Matrix[rows]X[cols]`, type-parameter binders in [`generics.md`](generics.md) §2.4) and removes the parenthesized form, which was the only place in the type system that used `()` for type-level grouping. The old `(ParamType, ...) -> ReturnType` form is **rejected** in every position outside the lambda-variable shorthand in §7.3.

**Old to new mapping.** Every old function-type form from [`syntax.md`](syntax.md) §2.9 maps to the new form:

| Old form | New form |
|---|---|
| `() -> Void` | `Void[]` |
| `(Int) -> Float` | `Float[Int]` |
| `(Int, Float) -> String` | `String[Int, Float]` |
| `(&Int) -> &Float` | `&Float[&Int]` |
| `(this Player) -> Float` | `Float[this Player]` |
| `(this Player, Int) -> Float` | `Float[this Player, Int]` |
| `(this Player, Int) mut -> Float` | `Float[this Player, Int] mut` |
| `(this Player, &Int) -> &Float ? AbortType` | `&Float[this Player, &Int] ? AbortType` |
| `(this Buffer[n]) -> Array[n]` | `Array[n][this Buffer[n]]` |

`mut` is position-sensitive: it always follows the brackets, never appears inside them, and is legal only when the first parameter is `this`. The brackets themselves are required even when the parameter list is empty. A method whose `this` type binds a type-parameter symbol (see [`generics.md`](generics.md) §4.3) produces a function type that nests the type-parameter slot of the receiver inside the same brackets as the parameter list, e.g. `Array[n] rowAt(this Buffer[n], i Int)` has function type `Array[n][this Buffer[n], i Int]`. The bracket-content delimiter rule from the function-overhaul design doc disambiguates: a bracket that contains `this`, a type name, or `&` is a function-type parameter list, not a type-parameter slot.

### 7.5 Lambdas do not capture
Lambdas **MUST NOT** capture outer variables. Every dependency must be passed as a parameter or supplied through surrounding storage explicitly. See [`concurrency.md`](concurrency.md) §5.2 ("Lambdas do not capture").

### 7.6 No bound method references
Zane does not provide bound method references as a separate feature. Because lambdas do not capture, there is no syntax that implicitly stores a receiver inside a function value. Code that needs a receiver later must keep that receiver in ordinary storage and pass it explicitly when the function value is invoked. The no-reference rule of §4.4 and §7.1 means a method is not a value either; only a lambda literal or lambda variable can carry a `this` receiver.

### 7.7 Call resolution with a lambda variable
The following walkthrough ties the overload-resolution mechanism of §5 to the type-fixity rule for lambda variables from §7.3. The scenario is a single lambda variable passed as an argument to an overloaded function:

```zane
Void receiver(Float[Int]) { ... }
Void receiver(Float[Float]) { ... }

callback (Int) -> Float(x Int) {
    return Float(x.value) * 2
}

Void main() {
    receiver(callback)
}
```

The walkthrough has three steps. First, `callback` is declared with the lambda-variable shorthand from §7.3. By the type-fixity rule, the variable's static type is fixed at first declaration as `(Int) -> Float` and is preserved across any subsequent reassignments of the body. The argument `callback` therefore has the single static type `(Int) -> Float` at every call site, regardless of which body the variable currently holds. Second, `receiver` is overloaded: two declarations with parameter types `(Int) -> Float` and `(Float) -> Float`. The overload-identity rule of §4.1 distinguishes the two unambiguously because their parameter types differ. Third, the call `receiver(callback)` enters the §5 resolution pipeline. The argument `callback` has static type `(Int) -> Float`. The direct-match phase (step 1 of §5) compares the argument type against each candidate's parameter type. The `(Int) -> Float` overload matches exactly. The `(Float) -> Float` overload does not match — there is no implicit conversion that turns `(Int) -> Float` into `(Float) -> Float` because function types have no implicit constructors in either direction. Resolution selects the first overload, and the call is unambiguous.

**Key insight.** The call is unambiguous not because the lambda variable is monomorphic — it is, but that is not what disambiguates the overload — but because the destination slot, the parameter of the selected overload, has a single function type. Function-typed parameters are monomorphic by the overload-identity rule of §4.1, so any argument passed to one is resolved by direct match against that one type. This is the property that makes lambda values safe to use in calls to overloaded functions.

---

## 8. Connection to the Effect Model

Read-only methods and free functions are effect-free with respect to their receiver unless they touch refs or capabilities. `mut` marks the only direct path for writing receiver-owned state. This is why overload identity ignores `mut`: the call contract is structurally the same even though the behavioral permissions differ.

> **See also:** [`effects.md`](effects.md) for the complete effect model and concurrency implications.

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| Methods are functions with `this` | Keeps the language model flat: methods are ordinary functions with one extra permission token. |
| `&` parameters in constructors and methods | An `&` field must be initialized from an allowed `&` source; requiring `&` on the corresponding parameter makes this constraint visible in the signature without ghost refs or hidden storage creation. |
| Plain `T` parameters are value-only | A caller is not required to supply a stable storage location for a plain parameter; restricting plain parameters from populating `&` fields prevents hidden dependency on call-site expression form. |
| `:` and `!` are distinct call markers | Makes mutation visible at the call site without adding mutable-reference types. |
| Subscripts are place projections only | Keeps `[]` predictable: an indexed expression always projects existing storage rather than running arbitrary computation. |
| No overloads that differ only by `&` | Call syntax stays uniform while avoiding overload ambiguity between value-only and place-required contracts. |
| Overload identity ignores `mut` | The call contract (parameter types and arity) is the same; `mut` only adjusts the permission granted to `this`, so it cannot disambiguate two otherwise-identical declarations. |
| Overload resolution phases: direct, generic, implicit | Makes implicit conversions a fallback after exact matches, preventing surprising behavior when an exact match exists. |
| Generic match uses inferred type generics only | Callers never write type arguments, so the generic-match phase is purely an inference step driven by argument types and type ascriptions (see [`generics.md`](generics.md) §5). |
| Generic match also unifies type-parameter symbols | Type-parameter symbols inside the called declaration are inferred from the type-parameter part of each call argument's static type in the same pass, so a single generic-match phase handles both kinds. The *type-parameter part* of a static type is the integer value baked into the type identifier (see [`generics.md`](generics.md) §2.4 and §7.1). |
| Functions are not nameable values | Makes Zane's function surface a 1:1 parallel of [`operators.md`](operators.md) §1 ("operators are grammar, not values"). Operators are a fixed set of grammar tokens, not user-defined values; function names are now the same. Overloaded names were already non-values; the rule unifies the overloaded and non-overloaded cases. |
| Lambda-variable shorthand parallels constructors | Parallels the existing constructor shorthand `text String("hello")` from [`syntax.md`](syntax.md) §1.1, fusing a variable name with a callable type and a body. The variable name, function type, and parameter names are all declaration-shaped, so the fusion is unambiguous. |
| Return-type-first function type syntax | Parallels the lambda literal syntax in §7.2 (`ReturnType(params DeclForm) { body }`). Putting the return type first reads as *"return type, parameterized by these input types"*, which is the natural reading order for a value the caller is going to use. |
| No lambda capture | Preserves explicit data flow and keeps effect analysis tractable. |
| Home-package-first method lookup | Makes unqualified method calls locally understandable and unaffected by imports. |

---

## 10. Summary

| Concept | Rule |
|---|---|
| Method | Package-scope function whose first parameter is `this` |
| `mut` method | Called with `!`; receiver MUST be a class; may mutate `this` and its owned subtree |
| Read-only method | Called with `:`; may read but not write `this` |
| Free function | Package-scope function without `this`; no private-field privilege |
| `&` method parameter | Caller must supply an allowed `&` source; callee may store into `&` fields |
| Plain `T` method parameter | Value-only; caller may supply a temporary; callee **MUST NOT** bind it into `&` storage |
| Subscript | Package-scope place projection written `(this T)[...] => placeExpr`; no explicit return type |
| Overload identity | Parameter types only; not names, return type, or `mut`; overloads differing only by `&` at one position are illegal |
| Overload resolution phases | Direct match, then generic match, then implicit match; ambiguity within any one phase is an error |
| Function reference | Illegal in all positions; a function name is not a value |
| Lambda literal | Either explicit-typed (`Float(x Int) { ... }`) or contextually-typed (`(x) { ... }`); contextual form requires a typed slot |
| Lambda variable | Shorthand for a typed lambda held in a single-type variable; type is fixed at first declaration, only the body may be reassigned |
| Function type | `ReturnType [paramTypes] mut? ?AbortType?`; brackets required even when empty |
| Method type | `Float[this Player]`, `Float[this Player, Int] mut`, `&Float[this Player, &Int] ? AbortType` |
| `mut` placement | Always after the brackets; legal only when the first parameter is `this` |
| `&` parameter | Inside the bracket: `Float[&Int]` |
| `&` return | `&Float[Int]` |
| No lambda capture | Unchanged from §7.5 |
| Unqualified method lookup | Searches home package, then current package |
| Extension methods | Any package may declare methods on imported types by naming the first parameter `this` |
