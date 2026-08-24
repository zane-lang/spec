# Zane Control Flow

This document specifies how Zane branches and repeats. Neither is a language construct: both are ordinary calls that take a **block argument**, declared by the `core` package over two compiler intrinsics. What the language itself contributes is the block, the intrinsics, `guard`, and the 1-based ordinal convention.

> **See also:** [`syntax.md`](syntax.md) §4.9 and §5 for the canonical surface syntax. [`operators.md`](operators.md) §2.4 for the `Bool` operators. [`types.md`](types.md) §2.6 for the fundamental types these calls are written in terms of.

---

## 1. Overview

Zane has no `if` statement and no `loop` statement. It has a way to hand a run of statements to a verb, and two intrinsics that a verb can use to run one conditionally or repeatedly. Everything a reader recognizes as control flow is built from those.

- **`Block arguments`.** A braced run of statements may be passed to a call. It captures its surroundings, cannot escape, and runs during the call.
- **`Branching and repetition are calls`.** `if`, `elif`, `else`, and counted repetition are `core` declarations, resolved and overloaded like any other verb.
- **`Two intrinsics`.** `@controlflow$branch` and `@controlflow$repeat` are the only primitives. They are stated over storage primitives, so they depend on no package, and callers reach them by ordinary coercion.
- **`Repetition is bounded by construction`.** `repeat` takes a count, so no control flow built on it can repeat without a written bound.
- **`guard` is grammar.** An exit cannot be a call, because a call is an expression inside the scope it would have to leave — which is also why no intrinsic exits a scope. Its condition is stated over a storage primitive, so it too depends on no package.
- **`1-based ordinals`.** Counted repetition and positional indexing start at `1`, not `0`.

---

## 2. Block Arguments

### 2.1 A block argument is a run of statements passed to a call
A **block argument** is a braced run of statements written at a call site and executed by the callee. Its type is the compiler concept type `@concepts$Block`, or `@concepts$Block<T>` when it yields a `T`.

```zane
ran Bool = if(ready) {
    start()
}
```

A block takes no parameters and has no name. It is not a lambda: a lambda is a self-typed function *value* with a complete written type ([`functions.md`](functions.md) §7.2), while a block is a source construct that never becomes a value.

> **See also:** [`syntax.md`](syntax.md) §4.9 for where a block may be written.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#a-block-is-not-a-lambda) — "A block is not a lambda".


### 2.2 A block captures, and does not escape
A block reads and writes the bindings of the scope it is written in, exactly as any other braced block does. Nothing is passed to it.

A block **MUST NOT** escape the call it is written at. It may not be stored, returned, bound to a symbol, placed in a field or element, or spawned. It may be handed on to another verb, because that call is still inside the original call's dynamic extent.

This non-escape is what makes capture safe here while it stays forbidden for lambdas ([`functions.md`](functions.md) §7.4, [`concurrency.md`](concurrency.md) §5.2). A lambda may be stored and run later, possibly in parallel, so captured state could be reached from somewhere the compiler cannot see. A block runs during the call that receives it, in the frame that wrote it.

A verb that declares a `@concepts$Block` parameter **MUST NOT** be spawned ([`concurrency.md`](concurrency.md) §3.1).
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#where-the-capture-would-have-bitten) — "Where the capture would have bitten".


### 2.3 A block is a scope for bindings, not for control transfer
A block owns the symbols declared inside it, and they are destroyed when it ends, like any other lexical block ([`lifetimes.md`](lifetimes.md) §2.1).

It is **transparent** to control transfer. `return`, `abort`, and `guard` written inside a block act on the scope containing the *call*, not on the block:

```zane
Int firstNegative(values IntList) {
    i Int = Int(1)
    i!to(values:size()) {
        guard values[i] < Int(0)
    }
    return i
}
```

The `guard` above leaves `firstNegative`'s body scope, so it ends the repetition rather than one pass of it. A `return` inside a block returns from the enclosing verb.

Transparency is what keeps an exit usable at any depth. A construct that opened a control-transfer boundary would trap every exit written inside it, which is the situation `guard` exists to avoid (§4).

Transparency is a property of the lowering, not a way to leave a frame. A verb that declares a `@concepts$Block` parameter is **expanded at its call site**, and so is every call it passes the block on to. A block therefore never crosses a call boundary at run time, and an exit written inside one is a jump within a single frame. This follows from what a block already is — no written type (§2.1), never a value, unable to escape or be spawned (§2.2) — so no frame other than the writing one can ever hold it.

There is no intrinsic that exits a scope, and none is needed. One would have to name a scope further up and unwind to it, which Zane does not do ([`error-handling.md`](error-handling.md) §4).
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#the-exit-that-could-not-become-a-call) — "The exit that could not become a call".


### 2.4 A block may yield a value
A `Block<T>` yields a `T`. Each of its yielding paths ends with `resolve`, which substitutes the value into the call that receives the block — the same keyword and the same meaning it carries in an abort handler ([`error-handling.md`](error-handling.md) §3.3).

```zane
ran!elif({ resolve cache:has(key) }) {
    use(key)
}
```

`return` inside a block still leaves the enclosing verb (§2.3). The two are not alternatives: `resolve` finishes the block, `return` finishes the verb.

---

## 3. Branching and Repetition

The declarations in this section are `core` declarations, not language constructs. They are named here because every Zane program uses them, and because their shapes are what the intrinsics of §5 are built to support. Any package may declare others (§5.3).

### 3.1 `if` starts a chain and reports whether it ran
`if(condition) { ... }` runs its block when the condition is `true`, and returns a `Bool` recording whether it did.

```zane
if(ready) {
    start()
}
```

The result need not be bound. An unbound value-type result is discarded ([`lifetimes.md`](lifetimes.md) §4), so a conditional that has no continuation is written as a statement.

### 3.2 `elif` and `else` continue the chain
A chain names the `if` result and continues it. `elif` runs its block when no earlier branch has run and its own condition holds, and it writes the chain value; it is therefore a `mut` method called with `!`. `else` runs its block when no earlier branch has run, and only reads the chain value, so it is called with `:`.

```zane
ran Bool = if(age > Int(18)) {
    print("adult")
}
ran!elif(age > Int(13)) {
    print("teenager")
}
ran:else() {
    print("junior")
}
```

Because a chain is a sequence of ordinary calls rather than one construct, its parts are joined by the value they pass along and nothing else. The compiler checks each call; it does not check that a chain is well-formed.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#the-last-thing-still-tied-to-a-package) — "The last thing still tied to a package".


### 3.3 A condition is evaluated unless it is written as a block
The condition of an `elif` is an ordinary argument and is evaluated before the call, like any other (§2.4 of [`operators.md`](operators.md) states the same for the `Bool` operators). A condition that must not run when an earlier branch already matched is written as a block, selecting the `Block<Bool>` overload:

```zane
ran!elif(cheapFlag) { ... }                        // evaluated
ran!elif({ resolve expensiveCheck() }) { ... }     // evaluated only if reached
```

Deferral is therefore visible at the call site rather than implied by the name of the construct.

### 3.4 Counted repetition advances ordinary storage
Counted repetition is a `mut` method on `Int`. The induction variable is an ordinary local that the block captures; the method advances it through the inclusive range.

```zane
i Int = Int(1)
i!to(Int(3)) {
    print(i)
}
```

The block above sees `i = 1`, then `2`, then `3`. Nothing is passed to the block, and no binding is introduced by the call; `i` is the caller's storage throughout.

### 3.5 There is no unbounded repetition
No `core` declaration repeats without a count, and none can be written (§5.2). A repetition whose real stopping condition is a test carries a bound anyway and stops with a `guard`:

```zane
attempt Int = Int(1)
attempt!to(maxTries) {
    guard connection:up()
    log(connection:ping())
}
```

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#doing-without-while) — "Doing without `while`".

---

## 4. `guard` Scope Exit

### 4.1 `guard` exits when its condition is true
`guard condition` immediately exits the current lexical scope when `condition` evaluates to `true`. When the condition is `false`, execution continues with the next statement in the same scope.

The condition is a coercion site with destination type `@primitives$Bool` ([`types.md`](types.md) §4.2). The destination is the storage primitive rather than a fundamental type, so the one construct the language keeps names no package's type. `core` bridges the two by declaring the implicit constructor from its `Bool` to that primitive, exactly as it declares the one from `@concepts$Number` to `Int` ([`types.md`](types.md) §2.6).

Zane defines no general truthiness rule: a type enters a `guard` only when it declares a single applicable implicit constructor to `@primitives$Bool`. Any type may declare one, so a `guard` is not confined to `core`'s `Bool`; the no-chaining rule ([`types.md`](types.md) §4.3) keeps that reach one step deep.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#the-exit-that-could-not-become-a-call) — "The exit that could not become a call".


```zane
{
    value Int(3)
    guard finished
    print(value)
}
```

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-that-opens-no-scope-of-its-own) — "An exit that opens no scope of its own".

### 4.2 `guard` may run a pre-exit block
`guard condition { ... }` first executes the attached block and then exits the same enclosing scope when the condition is `true`. If the condition is `false`, the attached block is skipped.

```zane
{
    value Int(3)
    guard shouldExit {
        print(value)
        print("pre-exit")
    }
    print("still inside")
}
```

The block attached to a `guard` is part of the `guard` grammar, not a block argument (§2).

### 4.3 `guard` is grammar because an exit cannot be a call
`guard` is the one control-flow construct the language keeps as grammar rather than a `core` declaration. With its condition stated over a storage primitive (§4.1), the language's entire control-flow surface — block arguments, the two intrinsics, and `guard` — names no declaration in any package. A call is an expression evaluated *inside* a scope, so it can never be the thing that leaves that scope; only a construct that opens no scope of its own and is not a call can exit the scope it sits in. For the same reason there is no exit intrinsic (§2.3).

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-that-opens-no-scope-of-its-own) — "An exit that opens no scope of its own".

---

## 5. Control-Flow Intrinsics

### 5.1 Two intrinsics, stated over storage primitives
The language provides exactly two control-flow operations:

```zane
@controlflow$branch(condition @primitives$Bool, body @concepts$Block)
@controlflow$repeat(count @primitives$Int, body @concepts$Block)
```

`branch` executes `body` when `condition` is true and does nothing otherwise; there is no fallback parameter, because the fallback case is `branch` on the complement. `repeat` executes `body` exactly `count` times; a `count` below `1` executes it zero times.

Both take **storage primitives** rather than the fundamental types. That is what separates control flow from the language: an intrinsic depends on no declaration in any package, so `core` is an ordinary consumer of them rather than a privileged part of the compiler.

An intrinsic is called like a function, so its arguments are coercion sites ([`types.md`](types.md) §4.2). A caller therefore writes ordinary values and never unwraps anything: a `Bool` reaches `branch` through the implicit constructor `core` declares to `@primitives$Bool`, the same conversion a `guard` condition uses (§4.1). A type that declares its own conversion is accepted on the same terms.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#two-intrinsics-and-what-they-are-stated-over) — "Two intrinsics, and what they are stated over".


### 5.2 Repetition is bounded by the shape of `repeat`
`repeat` takes a count, so a single invocation always terminates and every control-flow construct built on it carries a written bound, whoever declares it. The guarantee is a property of the intrinsic rather than of who may call it. It bounds one invocation, not a whole program: recursion can still re-enter `repeat` without limit, and it remains the language's only unbounded path.

An indefinite repetition is expressed by giving a ceiling and stopping inside the body (§3.5), or by a scheduling facility that names the recurrence as such.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#two-intrinsics-and-what-they-are-stated-over) — "Two intrinsics, and what they are stated over".


### 5.3 Any package may declare control flow
A package that wants a repetition policy, a branching form, or a scoped resource construct declares a verb taking a `@concepts$Block` parameter and calls the intrinsics, exactly as `core` does for §3.

```zane
Unit twice(body @concepts$Block) {
    @controlflow$repeat(Int(2), body)
    return Unit()
}

twice() {
    print("again")
}
```

The `@` namespaces are reachable from every package without an import ([`syntax.md`](syntax.md) §2.7), and the coercion of §5.1 supplies the primitive, so no package is closer to the intrinsics than any other. `core`'s declarations in §3 have no standing the example above lacks. Being the fundamental types' home package gives `core` first place in unqualified method lookup, not exclusive rights: any package may declare methods on them, reached by a qualifier where lookup does not find them ([`functions.md`](functions.md) §6.1 and §6.3).

---

## 6. 1-Based Ordinal Counting

### 6.1 Positional indexing is 1-based
When an `Int` identifies an ordinal position in an ordered sequence, the first position is `1`. For a sequence with `n` elements, the positional index range is therefore `1` through `n`.

```zane
lastPosition Int = list:size()
element Int = list[lastPosition]
```

The example above selects the last element by using the sequence size directly as the final valid position.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#counting-from-one) — "Counting from one".

### 6.2 Bounds behavior remains separate
This document specifies the ordinal base only. The language-level behavior for out-of-range element access remains a separate question from whether indexing starts at `0` or `1`.

---

## 7. Summary

| Concept | Rule |
|---|---|
| Block argument | A braced run of statements passed to a call; type `@concepts$Block` or `Block<T>`; no parameters, no name, never a value |
| Capture | A block reads and writes its enclosing scope's bindings |
| Escape | A block may not be stored, returned, bound, placed in storage, or spawned; it may be handed to another verb |
| Scope | A block owns its own declarations but is transparent to `return`, `abort`, and `guard`, which act on the scope containing the call |
| Lowering | A verb taking a block parameter is expanded at its call site, transitively, so a block never crosses a call boundary and an exit inside one is a jump within one frame |
| Yielding | A `Block<T>` ends its yielding paths with `resolve`; `return` still leaves the enclosing verb |
| Branching | `if` returns whether it ran; `ran!elif(...)` continues the chain and writes it; `ran:else()` ends it — all `core` declarations |
| Condition evaluation | An ordinary argument is evaluated; a `Block<Bool>` argument defers, and the choice is visible at the call site |
| Counted repetition | `i!to(end)` advances the caller's own `Int` and captures it in the block |
| Intrinsics | `@controlflow$branch` and `@controlflow$repeat`, stated over `@primitives$Bool` and `@primitives$Int`; reachable from any package, with ordinary values reaching them through the implicit constructors `core` declares |
| Bounded repetition | `repeat` takes a count, so one invocation always terminates and every construct built on it carries a written bound; recursion remains the only unbounded path |
| `guard` | The only control-flow grammar; exits the enclosing scope, opens no scope of its own, and may run a pre-exit block. Its condition coerces to `@primitives$Bool`, so it names no package's type |
| Ordinals | Positions and counted repetition start at `1`; the last valid position is the size |
