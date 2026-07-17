# Zane Functions, Methods, and Lambdas

This document specifies Zane's function model: functions, methods, subscripts, overload resolution, function values, lambdas, and method name resolution. Data declarations and constructors live in [`types.md`](types.md); the package-scope rules that host these declarations live in [`packages.md`](packages.md).

> **See also:** [`types.md`](types.md) §3 for constructors. [`memory.md`](memory.md) §2 for ownership and `&` rules. [`effects.md`](effects.md) §2 for `mut`. [`syntax.md`](syntax.md) §3 for declaration grammar.

---

## 1. Overview

Zane unifies methods, functions, and lambdas under one model: a callable is a package-scope declaration (or anonymous literal) whose first parameter may optionally be `this`.

- **`Verb`.** A **verb** is a callable whose body is a sequence of statements that executes to do work: functions, methods, operators, constructors, and lambdas (a lambda being an anonymous verb). The spec uses "verb" whenever a rule applies to all of these as a group, and reserves "function" for the narrow form — an ordinary identifier-named verb with no `this`. A subscript is not a verb — its body must be a place expression that projects a place rather than running computation (§2.9).
- **`Package-scope behavior`.** All methods, functions, and constructors are declared at package scope; type bodies never contain behavior.
- **`Methods as verbs`.** A method is a verb whose first parameter is `this`, so methods and functions share one model and differ only by the receiver.
- **`Capability markers`.** A verb's kind is selected by surface markers, and each marker unlocks a capability: naming the first parameter `this` grants private-field access (a method); naming the verb after a type grants `init{ }` and an implicit return type (a constructor). See §8.
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

A write to `this` lands on the caller's object; how `this` reaches the caller differs by kind (see [`memory.md`](memory.md) §2.9):

- For a **value-type** receiver, `this` is a **mutable borrow** of the caller's slot — the actual value, not a copy. This is what makes value types mutable in place, retiring the older pattern of returning a replacement. Because the borrow is scoped and non-escaping, the value keeps its value semantics: `this` may be read and written but cannot be stored as an `&` or returned as one, since a value type is not `&`-rootable.
- For a **reference-type** receiver, `this` is an implicit **`&` reference** to the object (never swallowed). A `mut` method mutates through it as through any `&`, and `this` composes with the `&` system — it may be passed where an `&T` is expected.

```zane
Void setScale(this Node, scale Float) mut {   // reference receiver
    this.scale = scale
}
```

```zane
Void setY(this Vec2, y Float) mut {           // value receiver: in-place through the borrow
    this.y = y
}

pos!setY(Float(3))
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
Explicit parameters other than `this` are read-only: they cannot be assigned or marked `mut`. Mutation of another object must be expressed as a `mut` method call on that object as the receiver. How each parameter is passed — a value borrow, or a reference `&`/swallow — is covered in [`memory.md`](memory.md) §2.9.

### 2.8 `&` and swallowing method parameters
A method parameter declared as `&T` is a **reference**: the caller supplies a source that may create a new `&` under [`memory.md`](memory.md) §2.8, and the callee may store it into an `&` field. A parameter declared as a plain reference type `T` **swallows** its argument — it takes the value by owning access, which the value's call-site scope keeps ([`lifetimes.md`](lifetimes.md) §1.5) — so it cannot be bound into `&` storage, because a swallowed value is owned at the call site while an `&` field may outlive the call (see [`memory.md`](memory.md) §2.9). A value-type parameter is a read-only borrow. To pass a reference object for reading only, use `&T`.

```zane
type Car = #struct {
    engine &Engine;
    _value Int;
}

// `&` parameter: may be stored into an `&` field
Void setEngine(this Car, engine &Engine) mut {
    this.engine = engine   // legal
}

// `&` parameter, read only
Int calculate(this Car, engine &Engine) {
    return this._value + engine.speed   // legal: reading through the reference
}

// plain reference-type parameter swallows; a swallowed owner is not an `&` source
Void setEngineWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: cannot store a swallowed owner into an `&` field
}
```

Call syntax is uniform regardless of the parameter mode:

```zane
engine Engine()
car!setEngine(engine)      // legal: engine may create a new `&`
car:calculate(engine)      // legal: read-only reference to engine
car!setEngine(Engine())    // ILLEGAL: temporary cannot bind to `&` parameter
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

---

## 5. Overload Resolution with Implicit Constructors

For function calls, constructor calls, and desugared method calls, overload resolution proceeds in three phases:

1. **Direct match.** A candidate is viable only if the call type-checks with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
2. **Generic match.** If the direct phase finds no viable candidate, the called declaration's inline-introduced type and number parameters are inferred from the static types of the call arguments: a type parameter from an argument's type, a number parameter from the number part of an argument's type. A call never carries a `<>` type-argument list; a type or number may instead be passed as an ordinary argument to a `Type` or `Number` value parameter (see [`generics.md`](generics.md) §5). Inference proceeds under the rules of [`generics.md`](generics.md) §3 and §5, still with no implicit constructor insertions. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.
3. **Implicit match.** If the direct and generic phases find no viable candidate, implicit constructors may be inserted at coercion sites. If exactly one candidate is viable, it is selected. If more than one candidate is viable, the call is an ambiguity error.

If no phase yields a viable candidate, the call is a normal no-match type error.

Implicit constructors are therefore a last resort. They never participate in discovering an otherwise unknown destination type for generic inference.

These phases describe **static** overload resolution. Matching a `variant` on its cases is not part of overload resolution at all; it is a separate construct — the `match` block — that dispatches on the live tag at runtime. See [`adt.md`](adt.md) §5.

> **See also:** [`types.md`](types.md) §4 for the definition and constraints of implicit constructors. [`adt.md`](adt.md) §5 for variant matching.

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

### 7.6 Generics are orthogonal to overloading for function values
A lambda is a single value with one exact type, even when that type is a function type (§7.2). Overload identity is parameter types only (§4.1), so a function type is a single, unique parameter shape. Passing a lambda to an overloaded callable is therefore an exact shape match at that parameter position, not a contest the lambda must win.

The circularity that makes overloaded **names** unusable as values (§7.1) does not apply to a lambda. An overloaded name is a candidate *set* with nothing to collapse it in value position; a lambda is already a single value. That distinction — not genericity — is what lets a self-typed lambda be passed to an overloaded callable while a bare callable name cannot.

A function value that is itself **generic** — polymorphic over its own type or number parameters — is **not specified in this version**. The open question is its runtime representation (monomorphization versus dictionary passing), which is a memory-model decision, not an overload-resolution or type-checking one: a generic function type would still be a unique parameter shape under the rules above. See [`generics.md`](generics.md) §9.

---

## 8. The Verb Model and Capability Markers

Every callable in Zane is a verb (§1). What *kind* of verb a declaration is — function, method, constructor, operator, or lambda — is decided entirely by a small set of surface **markers**, and each marker unlocks a specific capability. There is no separate mechanism per callable kind; there is one verb, and markers select what it may do.

### 8.1 One model, selected by markers

| Marker | Verb kind | Capability unlocked |
|---|---|---|
| First parameter named `this` | Method | Private-field access on the receiver; `:` / `!` call syntax |
| Name is a type | Constructor | Return type is the named type (no return annotation); `init{ }` for field **initialization** |
| Symbol name (operator token) | Operator | Operator-position calls |
| No name | Lambda | Anonymous function value |
| Plain identifier, none of the above | Function | No special capability |

The markers are largely independent — a lambda may still declare a `this` receiver (§7.2), for example — but the kinds above are distinguished by which markers are present. A constructor body and a method body are otherwise ordinary verb bodies (§2, §3).

### 8.2 `init{ }` is to constructors what `this` is to methods

The marker model makes the constructor/function relationship exact: **a constructor is a verb whose name is a type.** Naming a verb after a type does two things and nothing else — it makes the return type implicit (the verb produces the type it names) and it unlocks `init{ }` (see [`types.md`](types.md) §3). This mirrors methods precisely: naming the first parameter `this` is the only thing that makes a verb a method, and that token alone unlocks private-field access (§2.2).

So `init{ }` is a capability gated by a naming convention, exactly as `this` is. A plain function cannot use `init{ }` for the same reason it cannot read `_`-prefixed fields: it lacks the marker that grants the capability. A function that needs to build a value calls the constructor instead (see [`types.md`](types.md) §3).

### 8.3 What is shared, and what the markers change

All verbs share one parameter system (see [`generics.md`](generics.md) §3), one body grammar, one overload-resolution procedure (§5), and one effect model (§9). The markers do not touch any of these. Comparing functions, methods, and constructors, the markers change only two things: whether a return type is written, and which private-state capability (`this` private-field access or `init{ }` initialization) is granted. (The operator and lambda markers additionally change call syntax and value representation; see the §8.1 table.) Bringing functions and constructors "closer together" is therefore not a missing feature — they are already the same verb, separated only by the name-is-a-type marker.

> **See also:** [`syntax.md`](syntax.md) §3 for the declaration forms of each verb kind. [`types.md`](types.md) §3 for constructors and the `init{ }` expression. [`operators.md`](operators.md) §2.2 for operator declarations.

---

## 9. Connection to the Effect Model

Read-only methods and functions are effect-free with respect to their receiver unless they touch tethers or capabilities. `mut` marks the only direct path for writing receiver-owned state. This is why overload identity ignores `mut`: the call contract is structurally the same even though the behavioral permissions differ.

> **See also:** [`effects.md`](effects.md) for the complete effect model and concurrency implications.

---

## 10. Design Rationale

| Decision | Rationale |
|---|---|
| Methods are verbs with `this` | Keeps the language model flat: methods are ordinary verbs with one extra permission token. |
| Constructors are verbs named after their type | Naming a verb after a type implies its return type and unlocks `init{ }`, exactly as naming the first parameter `this` makes a method and unlocks private-field access. A constructor is a verb with one marker, not a separate mechanism. |
| `&` parameters in constructors and methods | An `&` field must be initialized from an allowed `&` source; requiring `&` on the corresponding parameter makes this constraint visible in the signature without ghost tethers or hidden storage creation. |
| Plain `T` parameters cannot populate `&` fields | A caller is not required to supply a stable storage location for a plain parameter, so a value parameter is a read-only borrow and a reference parameter is swallowed; restricting either from populating `&` fields prevents hidden dependency on call-site expression form. |
| `:` and `!` are distinct call markers | Makes mutation visible at the call site without adding mutable-reference types. |
| Subscripts are place projections only | Keeps `[]` predictable: an indexed expression always projects existing storage rather than running arbitrary computation. |
| No overloads that differ only by `&` | Call syntax stays uniform while avoiding overload ambiguity between plain and place-required contracts. |
| Overload identity ignores `mut` | The call contract (parameter types and arity) is the same; `mut` only adjusts the permission granted to `this`, so it cannot disambiguate two otherwise-identical declarations. |
| Overload resolution phases: direct, generic, implicit | Makes implicit conversions a fallback after exact matches, preventing surprising behavior when an exact match exists. |
| Generic match infers type parameters | A call carries no `<>` list, so the generic-match phase is purely an inference step driven by the argument types (see [`generics.md`](generics.md) §5). |
| One pass handles types and numbers | Both type parameters and number parameters of the called declaration are inferred from the argument types in the same pass, since the two kinds share one inline introduction form (see [`generics.md`](generics.md) §3). |
| Callables are call-only | An overloaded name is a candidate set, not a value; it only collapses when arguments are supplied at a call site. Keeping methods, functions, and operators call-only removes the ambiguity of passing an overloaded name into an overloaded parameter, exactly as operators already avoid it. |
| Self-typed lambdas | A lambda carries its own complete type, so it is a single value with one exact type. The circularity that bars overloaded *names* from value position (a candidate set with nothing to collapse it) never arises, so a lambda matches an overloaded callable by exact shape — even if that type were a generic function type. |
| Generic function values deferred | Overloading and type-checking already handle a generic function type as a unique parameter shape; what remains open is its runtime representation (monomorphization versus dictionary passing), a memory-model decision left unspecified rather than an overload-resolution problem. |
| Lambda-variables for function values | A named lambda-variable has one function type and cannot accumulate an overload set, so it is always unambiguous in value position. |
| No lambda capture | Preserves explicit data flow and keeps effect analysis tractable. |
| Home-package-first method lookup | Makes unqualified method calls locally understandable and unaffected by imports. |

---

## 11. Summary

| Concept | Rule |
|---|---|
| Verb | A callable; its kind is selected by markers, and each marker unlocks a capability |
| Capability markers | `this` first → method (private access); name is a type → constructor (`init{ }`, implicit return); symbol name → operator; no name → lambda |
| Method | Package-scope verb whose first parameter is `this` |
| `mut` method | Called with `!`; a value-type `this` is a mutable borrow of the caller's slot, a reference-type `this` is an implicit `&` reference; may mutate `this` and its owned subtree in place |
| Read-only method | Called with `:`; may read but not write `this` |
| Function | Identifier-named package-scope verb without `this`; no private-field privilege |
| `&` method parameter | Caller must supply an allowed `&` source; callee may store into `&` fields |
| Plain `T` method parameter | Value-only; caller may supply a temporary; callee **MUST NOT** bind it into `&` storage |
| Subscript | Package-scope place projection written `(this T)[...] => placeExpr`; no explicit return type |
| Overload identity | Parameter types only; not names, return type, or `mut`; overloads differing only by `&` at one position are illegal |
| Overload resolution phases | Direct match, then generic match, then implicit match; ambiguity within any one phase is an error |
| Callable reference | Illegal; methods, functions, and operators are call-only and have no value form |
| Lambda | Self-typed function value: explicit parameter types, return type, abort type, and `mut`; no capture |
| Lambda-variable | Symbol bound to a lambda literal; has one function type; the only way to hold a function value |
| Generic function value | Not specified in this version; deferred on runtime-representation grounds, not overloading (see [`generics.md`](generics.md) §9) |
| Unqualified method lookup | Searches home package, then current package |
| Extension methods | Any package may declare methods on imported types by naming the first parameter `this` |
