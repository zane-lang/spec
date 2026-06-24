# Rationale: Generics and Type Parameters

Design journal for [`spec/generics.md`](../spec/generics.md). This file records the *why* behind the type-parameter system: the forks that forced a decision, the alternatives weighed, the reasoning that settled each one, and the costs we knowingly took on.

Entries are dated and append-only. When a decision changes, add a new entry rather than rewriting the old one — this file is a history of thinking, not a definition. The normative rules live in the spec; nothing here overrides them.

> **See also:** [`writing-rationale-docs.md`](../contributing/writing-rationale-docs.md) for the format of this file. [`spec/generics.md`](../spec/generics.md) for the rules these entries justify.

---

## Types are templated functions
**Spec:** [`generics.md`](../spec/generics.md) §2 · **Settled:** 2026-06

The fork was whether templating is a *feature* bolted onto the type system or a *consequence* of it. Most languages take the first road: types are one kind of thing, and a separate generics sublanguage (`template<...>`, `<T>`, `[T any]`) is grafted on with its own scoping, its own inference, and its own corner cases.

We had already committed to types being ordinary compile-time values that the compiler *executes* in an earlier stage. Given that, a parameterized type is just a function from parameters to a layout — and "generics" stops being a thing we add. It is what you get for free once a type is an executable value. `Vector<Int>` is not special syntax; it is applying an argument to a function and running it at compile time.

❌ A dedicated generics sublanguage (C++ templates, Java/Rust `<T>` grammar layered on a non-value type system). Rejected because it duplicates machinery the compile-time-value model already has, and because every such system grows its own scoping and inference rules that drift from the rest of the language.

The payoff is conceptual economy: one model (types are staged functions) explains parameterization, the `<>`/`()` split, and the literal-wrapping rule below. The cost is that the model *promises* more than the current spec delivers — if types are really functions, you would expect to run arithmetic on number parameters (`Array<T, rows * cols>`). We don't yet. See **Deferred** below; the gap between the promise and the delivery is real and worth naming honestly.

---

## The parameter ladder: a header for types, inline introduction for verbs
**Spec:** [`generics.md`](../spec/generics.md) §3 · **Settled:** 2026-06

This is the central decision of the whole document, and the one a one-line rationale table did the most damage to. It deserves the long version.

### The fork

A parameter has to be *introduced* somewhere. Two sites were on the table, and the temptation was to pick one and impose it everywhere for uniformity:

- a `<>` **header** after the name (`Foo<T Type>`), the way nearly every language headers its generic functions and types alike; or
- **inline**, at the parameter's first marked occurrence in the signature (`x T Type`), with no header at all.

Forcing one form on both types and verbs is the obvious "consistent" choice. We rejected it, because types and verbs are not doing the same thing.

### The thing that decides it: applied vs. inferred

A **type** is applied *positionally* by its users: `Vector<Int>`, `Buffer<Int, 64>`. The order of its parameters is part of its public interface — it is exactly what a use site fills, left to right. A public, ordered interface wants an explicit, ordered signature. That is what a `<>` header is. So types keep the header.

A **verb** (function, method, constructor) never gets applied positionally. Its parameters are always *inferred* from the value arguments, or passed as ordinary values. There is no order for a caller to fill, because a caller never writes `<...>` on a verb at all. So a header on a verb would declare an order nobody uses — pure ceremony. The verb introduces each parameter inline instead, at its first marked occurrence, and references it bare elsewhere.

The split is not stylistic. It tracks the real apply-versus-infer distinction. Header where there is a positional interface; inline where there is not.

❌ Header on everything (Rust/Swift style: `fn head<T, const N>(...)`). Clean to read because the binders come first, but it forces verbs to declare an order that no call site ever fills, and it reintroduces the `<>`-at-call-site channel we wanted to delete (see the next entry).

❌ Inline on everything, types included. Breaks the positional public interface of a type — you could no longer see `Vector`'s parameter order at a glance, and `Vector<Int>` would have no signature to check against.

### The ladder — why `x T Type` and `T Type` are the same idea at different heights

The inline form has a subtlety that the spec states mechanically ("first marked occurrence") but never *motivates*: why does `x T Type` mean "infer `T`" while `T Type` means "pass `T`"? They look almost identical — the second is the first with the value name deleted.

The motivating picture is a ladder of value:type rungs. Every binding names a rung, and the concept (`Type`/`Number`) is simply the type of the rung one level up:

```zane
Void func (x Int)        // x : Int(3) : Int                     — name rung 0
Void func2(x T Type)     // x : Int(3) : T : Int : Type          — name rung 0 AND rung 1
Void func3(  T Type)     //          T : Int : Type              — name rung 1 only
```

- `func(x Int)` names the ground value. `x` is `Int(3)`; its type is the concrete `Int`.
- `func2(x T Type)` names *two* rungs. `x` is the value; its type is `T`; and `T`'s own value is `Int`, whose type is the concept `Type`. The call `func2(Int(3))` gives the compiler `Int(3)`; it walks **up one rung** to recover `T = Int`. That upward walk *is* inference.
- `func3(T Type)` names only the upper rung. There is no ground value to bind, so the caller hands `T` directly: `func3(Int)`. That *is* explicit passing.

So inferring is not a different mechanism from passing — it is passing observed **one level lower**. The presence or absence of the leading value name is not an arbitrary mode flag; it is the answer to "do you also want to bind the rung below `T`?" Add the name, the compiler reads the type off a value (infer). Drop it, the caller supplies the type itself (pass).

This reframing earns its keep three ways:

1. It makes the `x T Type` / `T Type` distinction *legible* instead of arbitrary. A reader who holds the ladder reads `T Type` instantly as "the tower truncated by one rung."
2. It explains the literal-wrapping rule for free (see the next-but-one entry): inference is an up-one-rung read, and the up-rung of a bare literal is a *concept*, not a concrete type.
3. It turns the apparent footgun — "delete one identifier, flip the meaning" — into a legible edit: removing the name doesn't flip a flag, it stops binding the rung below. Still a sharp edge, but a sharp edge that *means* something.

### Costs / deferred

- **Non-local reading.** A bare reference may appear before its marked introduction: in `T head(arr Array<T Type, n Number>)` the return `T` is bound by the marked occurrence *later* in the signature. You cannot resolve a verb's parameters strictly left-to-right; you scan the whole signature for the introductions. The ladder does not fix this — it is orthogonal to infer-vs-pass — and it is the part of the design most likely to trip up newcomers. We accept it as the price of dropping the header.
- **The ladder is a `Type` story; `Number` is asymmetric.** `func2`-style inference works because a type can *be* the type of a value, so there is a rung 0 to name. A number cannot be the type of a value — `x n Number` is ill-formed — so `Number` has no flat infer form. Numbers are inferred only *structurally*, through a nested type like `Array<T Type, n Number>` where `n` rides on the literal's length, not on any value's type. The elegant three-rung symmetry is a property of `Type` specifically. Worth teaching where it stops.
- **The model is the load-bearing thing, not the syntax.** If a reader is taught the ladder, the syntax is fine. If they are not, `T Type` in a value list looks like a type sitting where a value should be. This is an argument for teaching the ladder up front (it is why this entry exists), not for changing the surface form.

---

## Parameters are concept-typed (`Type` / `Number`)
**Spec:** [`generics.md`](../spec/generics.md) §3.2 · **Settled:** 2026-06

Once a type handed to a constructor is just a compile-time value, its parameter needs a *type* like any other value — and we already had concept types (`@concepts$Number`, etc.) for exactly the "compile-time, parameter-position-only, never storage" role that literals occupy before lowering.

So a type parameter is declared with the `Type` concept and a number parameter with the `Number` concept, reusing that machinery wholesale. No bespoke parameter-kind keyword (`typename`, `const`, `comptime`) is needed.

❌ A dedicated keyword per parameter kind. Rejected as redundant: it would invent a second way to say "compile-time, parameter-position-only" when concept types already say exactly that.

**Costs / deferred:** the scheme cleanly distinguishes exactly two kinds, `Type` and `Number`, and leans on casing (uppercase type, lowercase number) to tell them apart at reference sites. If a third kind of compile-time parameter ever arrives — the deferred generic *function value* is the obvious candidate — the two-kind casing scheme may not extend without strain. Noted, not solved.

---

## `<>` describes architecture, `()` constructs — and calls never take `<>`
**Spec:** [`generics.md`](../spec/generics.md) §4–§5 · **Settled:** 2026-06

These are two rows in the old table but one decision. `<>` belongs to the type system: it is a compile-time, structural description of what a value's architecture *is*, resolved in an earlier compilation stage. `()` belongs to the value system: it constructs or runs at run time. Keeping them as different *mechanisms* — not two syntaxes for one idea — is what keeps each simple.

The sharp consequence is that a call **never** carries a `<>` list. There is no `Vector<Int>(...)` turbofish. A parameter reaches a callable either by inference (the ladder, up one rung from a value argument) or as an explicit `Type`/`Number` value argument. A parallel `<>` channel at the call site would be a third way to pass the same information, redundant with both.

❌ Turbofish / explicit type arguments at calls (Rust's `::<T>`, C++'s `f<T>()`). Rejected for redundancy and for the parser cost it forces elsewhere. But it has a real downside we are choosing to eat — see costs.

This decision is what makes case-sensitive parsing pay off: `Vector<Int>` is a type application and `a < b` is a comparison, told apart purely by whether the token before `<` is uppercase. Most languages pay for `<>` generics with permanent parser pain (the `>>` token, the most-vexing-parse); we don't, *because* `<>` lives only in type expressions and the casing rule disambiguates them. (See [`lexical.md`](../spec/lexical.md) §5.)

**Costs / deferred:** with no turbofish, a caller cannot force a type that appears only in a function's *return* (parse-to-`T`, empty-container-of-`T`, zero/default construction). The author must anticipate it and expose an explicit `Type` value parameter. Expressiveness that Rust gives the caller, we give the library designer. The deferred "phantom type parameters" item is the visible edge of this. We judge the simplicity worth it, but it is a genuine transfer of power, not a free win.

---

## Concept-typed literals must be wrapped at a call
**Spec:** [`generics.md`](../spec/generics.md) §5.4 · **Settled:** 2026-06

`Vector(Int(2), Int(3))` is legal; `Vector(2, 3)` is not. A bare `2` carries the concept type `@concepts$Number`, not a concrete `Int`/`Float`, and the compiler will not guess. So a bare literal must not drive inference of a type parameter.

This looks like an ad-hoc restriction until you read it through the ladder. Inference is an up-one-rung walk: from the value, to its type, which becomes `T`. The up-rung of `Int(3)` is the concrete `Int` — usable as `T`. The up-rung of a bare `2` is the *concept* `@concepts$Number` — not a concrete type, so there is nothing usable to assign to `T`. The wrapping requirement is not a special case; it is "the up-rung read has to land on a concrete type, and a bare literal's doesn't."

❌ Inferring a default concrete type from a bare literal (e.g. literal `2` ⇒ `Int`). Rejected: it bakes a silent type choice into every generic call and is exactly the kind of implicit decision the language avoids elsewhere.

**Costs / deferred:** one explicit `Int(...)` wrap at each such call site. This is the deliberate price that replaces a `<>` argument list *everywhere* else — a per-literal cost, paid once, in exchange for deleting the whole turbofish channel.

---

## Size is part of the type (`Array<T, n>`)
**Spec:** [`generics.md`](../spec/generics.md) §7 · **Settled:** 2026-06

The tempting shortcut is to leave an array's size out of its type — the stack pointer is just a register, and an array never resizes after construction. C99 VLAs do exactly this for locals, so there is prior art for "runtime-sized local is fine."

We bake the size in anyway, because the real cost of a runtime-sized *type* is not stack allocation — it is the **loss of uniform stride**. If two values of one type can differ in size, then `arr[i]` can no longer be `base + i * stride` with a constant stride; embedding the type in a struct leaves the outer layout unknown; copying needs a runtime size query; and calling conventions, which assume fixed-size parameters, break. Worse, the break *propagates*: an array of variable-size structs loses uniform stride, a struct containing one loses it, and so on up every containment chain.

❌ Runtime-sized array types (VLA-style, size out of the type). Rejected: it trades a tiny allocation convenience for stride loss that contaminates every type built on top.

So `Array<T, n>` is the mechanism that *guarantees* every value of a given type is the same number of bytes — and that guarantee is precisely what makes indexing, copying, embedding, and calling cheap. This is the clearest case in the whole language of the general bargain: a high-level convenience is forbidden so that a low-level guarantee holds. The strictness is not a tax paid alongside the performance; it *is* the performance.

> This rule is cited from [`adt.md`](../spec/adt.md) §4 — a directly inline self-referential type would have infinite size, which uniform stride forbids, which is why recursive types must box through `&`.

**Costs / deferred:** the size is in the type, so two arrays of different lengths are different types. Arithmetic on the size in a type position (`Array<T, rows * cols>`) is therefore a type-identity question, and is deferred — see below.

---

## `Array<T, n>` is the single storage primitive
**Spec:** [`generics.md`](../spec/generics.md) §8 · **Settled:** 2026-06

One compiler-provided fixed-size base case (`n` contiguous `T`) keeps the compiler's layout responsibility minimal. Every other fixed-size container — vectors, matrices — is defined in terms of `Array` and needs no extra compiler support. Dynamic containers, when specified, are separate runtime-managed wrappers over opaque storage, not extensions of `Array`.

❌ Multiple compiler-blessed container primitives. Rejected: each one is layout surface area the compiler must own; one base case plus library composition is smaller and more honest about what is primitive.

---

## Deferred: what the model promises but does not yet deliver
**Spec:** [`generics.md`](../spec/generics.md) §9 · **Settled:** 2026-06 (open)

Honest record of the gaps. The "types are executed functions" framing is elegant, and part of that elegance is a promise the current spec does not fully keep. Naming the gaps is the point of this entry.

- **Type-level arithmetic on number parameters** (`Array<T, rows * cols>`). The model says a type is a function you run at compile time, so running `*` on two numbers in a type position *should* fall out for free. It does not — because two type expressions being "the same type" requires a type-level equality rule that canonicalizes such expressions, and that is exactly the hard part the framing papers over. This is why const-generic arithmetic took years elsewhere. Deferred pending that equality rule. The elegant framing slightly oversells how much is free; this is where the bill comes due.

- **Constraints / bounds on type parameters.** `add(x T Type, y T Type)` silently assumes `T` supports `+`, but nothing yet lets an author *require* "T is addable / comparable / printable," nor pins down how the compiler checks it. As specified, the check is structural-at-instantiation (C++ pre-concepts templates), so a missing capability surfaces deep in the callee's body rather than at the signature. Bounded polymorphism is half of what makes generics usable in practice, and it is the most important thing to design next. Likely it grows out of the concept/effect machinery rather than a new sublanguage — consistent with "concepts are already how we type compile-time parameters."

- **Generic function values.** A function *value* polymorphic over its own type/number parameters is unspecified. The open question is runtime representation (monomorphization vs. dictionary passing) — a memory-model decision, not an overload-resolution one, since a generic function type is still a unique parameter shape. See [`functions.md`](../spec/functions.md) §7.6.

- **Phantom type parameters** — an introduced parameter with no path from any value argument, receiver, or literal that fixes it. With no turbofish, there is currently no way to supply one; this is the visible edge of the no-`<>`-at-calls decision above.

- Named lane access (`.x`/`.y`/`.z`/`.w`) and element-access bounds-checking rules are deferred on their own merits, not for model reasons.
