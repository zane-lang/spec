# Rationale: Generics and Type Parameters

> **See also:** [`spec/generics.md`](../spec/generics.md) for the rules these stories explain.

---

## Types are templated functions
**Spec:** [`generics.md`](../spec/generics.md) §2

The first fork was whether templating is a *feature* bolted onto the type system or a *consequence* of it. Most languages take the first road: types are one kind of thing, and a separate generics sublanguage (`template<...>`, `<T>`, `[T any]`) is grafted on with its own scoping, its own inference, and its own corner cases. We could have done the same — a dedicated generics grammar layered on a non-value type system, the way C++, Java, and Rust do — but it duplicates machinery the rest of the language already has, and every such system grows its own scoping and inference rules that drift away from the language around them.

We had already committed to types being ordinary compile-time values that the compiler executes in an earlier stage. Once that is true, a parameterized type is just a function from parameters to a layout, and "generics" stops being a thing we add — it is what you get for free. `Vector<Int>` is not special syntax; it is applying an argument to a function and running it at compile time.

The payoff is conceptual economy: one idea — types are staged functions — explains parameterization, the `<>`/`()` split, and the literal-wrapping rule that comes later. The honest cost is that the idea promises more than the spec currently delivers. If types are really functions, you would expect to run arithmetic on number parameters, `Array<T, rows * cols>`, and we cannot yet; the gap between that promise and the delivery is real, and the deferred-features story at the end of this file owns it.

---

## The parameter model: a header for types, inline introduction for verbs
**Spec:** [`generics.md`](../spec/generics.md) §3

This is the central decision of the whole document, and the one a one-line rationale table did the most damage to, so it gets the long telling.

A parameter has to be introduced somewhere, and there were two places to put it: a `<>` header after the name (`Foo<T Type>`), the way nearly every language headers its generic functions and types alike; or inline, at the parameter's first marked occurrence in the signature (`x T Type`), with no header at all. The tempting move is to pick one and impose it everywhere, for uniformity. We rejected that, because types and verbs are not doing the same thing.

A type is applied positionally by its users — `Vector<Int>`, `Buffer<Int, 64>`. The order of its parameters is part of its public interface; it is exactly what a use site fills, left to right. A public, ordered interface wants an explicit, ordered signature, and that is what a `<>` header is, so types keep the header. A verb — function, method, constructor — is never applied positionally. Its parameters are always inferred from the value arguments or passed as ordinary values, and a caller never writes `<...>` on a verb at all. A header there would declare an order nobody uses, pure ceremony, so the verb introduces each parameter inline instead and references it bare elsewhere. The split is not stylistic; it tracks the real apply-versus-infer distinction — header where there is a positional interface, inline where there is not. Forcing the header onto everything, the Rust/Swift `fn head<T, const N>(...)` style, reads cleanly because the binders come first, but it makes verbs declare an order no call site fills and quietly reintroduces the `<>`-at-call-site channel we wanted to delete; forcing inline onto everything breaks a type's positional interface, so `Vector<Int>` would have no signature to check against.

That leaves the subtlety the spec states mechanically ("first marked occurrence") but never motivates: why does `x T Type` mean "infer `T`" while `T Type` means "pass `T`"? The two look almost identical — the second is the first with the value name deleted. The idea that makes it click is that every symbol has a value and a type, and a type is itself a symbol — its value is a concrete type, its type is the concept `Type`. Take three verbs that differ only in how they declare their one parameter:

```zane
Void func (x Int) {}
Void func2(x T Type) {}
Void func3(T Type) {}
```

Now call each, and draw the parameter as a tree of its `value` and its `type`, expanding any `type` that is itself a symbol one level further:

```text
func(Int(3))
└─ x
   ├─ value: Int(3)
   └─ type:  Int

func2(Int(3))
└─ x
   ├─ value: Int(3)
   └─ type:  T
      ├─ value: Int
      └─ type:  Type

func3(Int)
└─ T
   ├─ value: Int
   └─ type:  Type
```

`func2` is the whole point. The parameter `x` has type `T`, and `T` is itself a symbol whose value — `Int` — is exactly what inference recovers: the same `value`/`type` subtree as `func3`'s `T`, only reached from a value argument instead of handed over directly. So inferring is not a separate mechanism from passing. It is the same `T Type` symbol with its value left blank, filled from the value argument instead of given at the call. `func(x Int)` leaves nothing open. `func3(T Type)` declares the type-symbol `T` and the caller supplies its value, `func3(Int)` — explicit passing. `func2(x T Type)` declares `x` with type `T` but says nothing about `T`'s value, so the call `func2(Int(3))` gives a value for `x` and `T` is just that value's type — inference. The presence or absence of the leading value name is the whole switch: it is the answer to "do you supply a value `T` can be read from, or do you supply `T`'s value yourself?"

Seeing it this way pays off three times over. It makes the `x T Type` / `T Type` distinction legible instead of arbitrary — once `T` is just a symbol whose value is a type, `T Type` reads as the same declaration as `x Int`, one level up. It explains the literal-wrapping rule for free, since inferring `T` means reading a value's type and a bare literal's type is a concept rather than a concrete type (the story on wrapping, later, picks this up). And it turns the apparent footgun — delete one identifier, flip the meaning — into a legible edit: removing the name does not flip a mode flag, it stops supplying a value for `T` to be read from. Still a sharp edge, but one that means something.

None of this is free. A bare reference can appear before its marked introduction — in `T head(arr Array<T Type, n Number>)` the return `T` is bound by the marked occurrence later in the signature — so you cannot read a verb's parameters strictly left to right; you scan the whole signature for the introductions. The value/type framing does not fix that; it is orthogonal to infer-versus-pass, and it is the part most likely to trip up a newcomer, the price we pay for dropping the header. The framing is also a `Type` story, not a `Number` one: a type can be the type of a value, so a type parameter's value can be read off an argument, but a number cannot be the type of a value — `x n Number` is meaningless — so numbers have no value-driven inference and are recovered only structurally, from a nested type like `Array<T Type, n Number>` where `n` rides on the literal's length. The symmetry between `x T Type` and `T Type` belongs to `Type` specifically, and it is worth teaching where it stops. Above all, the model is the load-bearing thing, not the syntax: a reader who has been told that a type is a symbol with a value and a type finds `T Type` natural; a reader who has not finds a type sitting where a value should be. That is an argument for teaching the idea up front — which is why this story exists — not for changing the surface.

---

## Parameters are concept-typed (`Type` / `Number`)
**Spec:** [`generics.md`](../spec/generics.md) §3.3

Once a type handed to a constructor is just a compile-time value, its parameter needs a type like any other value — and we already had concept types (`@concepts$Number` and the rest) for exactly the "compile-time, parameter-position-only, never storage" role that literals occupy before lowering. So a type parameter is declared with the `Type` concept and a number parameter with the `Number` concept, reusing that machinery wholesale, with no bespoke parameter-kind keyword like `typename`, `const`, or `comptime`. Inventing such a keyword would only be a second way to say "compile-time, parameter-position-only" when the concept types already say it.

The scheme cleanly distinguishes exactly two kinds, `Type` and `Number`, and leans on casing — uppercase type, lowercase number — to tell them apart at reference sites. The strain it has not yet faced is a third kind of compile-time parameter; the deferred generic *function value* is the obvious candidate, and the two-kind casing scheme may not stretch to cover it gracefully. Noted, not solved.

---

## `<>` describes architecture, `()` constructs — and calls never take `<>`
**Spec:** [`generics.md`](../spec/generics.md) §4–§5

These were two rows in the old table but they are one decision. `<>` belongs to the type system: a compile-time, structural description of what a value's architecture is, resolved in an earlier stage. `()` belongs to the value system: it constructs or runs at run time. Keeping them as different mechanisms — not two syntaxes for one idea — is what keeps each simple, and the sharp consequence is that a call never carries a `<>` list. There is no `Vector<Int>(...)` turbofish; a parameter reaches a callable either by inference, reading `T`'s value off a value argument, or as an explicit `Type`/`Number` value argument. A parallel `<>` channel at the call site would be a third way to pass the same information, redundant with both, which is why Rust's `::<T>` and C++'s `f<T>()` are deliberately not available here.

This is also what makes case-sensitive parsing pay off: `Vector<Int>` is a type application and `a < b` is a comparison, told apart purely by whether the token before `<` is uppercase. Most languages pay for `<>` generics with permanent parser pain — the `>>` token, the most-vexing-parse — and we do not, precisely because `<>` lives only in type expressions and the casing rule disambiguates them (see [`lexical.md`](../spec/lexical.md) §5).

The downside we are choosing to eat is that, with no turbofish, a caller cannot force a type that appears only in a function's return — parse-to-`T`, an empty container of `T`, zero or default construction. The author has to anticipate that and expose an explicit `Type` value parameter, so expressiveness Rust gives the caller, we give the library designer instead. The deferred "phantom type parameters" question is the visible edge of this. We judge the simplicity worth it, but it is a genuine transfer of power, not a free win — and how a caller names such a type at all, without the channel we just deleted, is taken up in the story on passing a type directly.

---

## Passing a type directly, without a turbofish
**Spec:** [`generics.md`](../spec/generics.md) §5.3

This story only exists because of the no-turbofish decision. Once we had decided that a call carries no `<>` list — no `Vector<Int>(...)`, no turbofish — inference became the only way a type reached a callable. For most calls that is enough: the type rides in on a value argument and the compiler reads it off (this is the parameter-model story). But not every type a call needs is sitting on a value. A function that only *returns* a `T` — a zero-initialised buffer, an empty container, a `parse` that turns text into an `Int` — has no value argument carrying the type, so there is nothing for inference to read. With the turbofish gone, the caller had no channel left to name that type at all. That is not a small gap: without an answer, `zeros(Int, 1024)`, an empty `List(String)`, and `parse(Int, text)` would simply be unwritable, and the no-turbofish decision would have quietly amputated a whole class of APIs. So we needed a way for the caller to hand a type over directly.

The first idea was to carve out a narrow exception — allow a turbofish *only* on the parameters inference cannot reach. We dropped it fast: it reintroduces the exact `<>`-at-the-call-site channel we had just deleted, brings back the parser ambiguity the casing rule had bought us out of, and leaves a call site that sometimes carries `<>` and sometimes doesn't — the worst of both worlds, and a rule no one could hold in their head. The second idea was a dedicated marker — a keyword or sigil in the `()` list that flags "this argument is a type." That was less bad, but still a bespoke mechanism, a third thing to learn, and it quietly concedes that a type is a second-class kind of argument that needs its own syntax to travel.

The answer that held needed no new channel at all, because the value/type model had already removed the need for one. A type is an ordinary compile-time value — the heart of the parameter-model story — a value travels in the `()` argument list; therefore a type can travel in the `()` argument list, as a value parameter whose declared type is the concept `Type`. That is all `func3(T Type)` is — a parameter named `T` of type `Type` — and the caller passes it like any other value: `func3(Int)`, or `Array(Int, 10000)` handing over a type and a size side by side. Nothing was added to the language; we just noticed that the channel we already had was enough. And because the author picks infer-versus-pass purely by the parameter's *shape* — a value name in front (`x T Type`) means "infer me," the bare concept (`T Type`) means "pass me" — there is still exactly one calling convention, not a value channel beside a type channel.

This is what makes "no turbofish" affordable instead of crippling, and it is the real reason passing a plain type matters: the previous decision deleted the type-argument channel, and this one shows the channel was never needed, because types were values all along — so none of the APIs that decision threatened are actually lost. The residual cost is the one the no-turbofish story named from the other side: it falls to the library author to expose an explicit `Type` parameter wherever inference cannot reach, and a parameter that *no* path — value, receiver, or literal — can fix is simply unsupplied. That open edge is the deferred "phantom type parameters" question.

---

## Concept-typed literals must be wrapped at a call
**Spec:** [`generics.md`](../spec/generics.md) §5.4

`Vector(Int(2), Int(3))` is legal; `Vector(2, 3)` is not. A bare `2` carries the concept type `@concepts$Number`, not a concrete `Int` or `Float`, and the compiler will not guess, so a bare literal must not drive inference of a type parameter. This looks like an ad-hoc restriction until you read it through the value/type idea: inferring `T` means taking a value and reading its type. The type of `Int(3)` is the concrete `Int`, usable as `T`; the type of a bare `2` is the concept `@concepts$Number`, which is not a concrete type, so there is nothing to assign to `T`. The wrapping rule is not a special case — it is simply that the type read off the argument has to be concrete, and a bare literal's isn't. Letting the compiler pick a default concrete type instead, `2` quietly becoming `Int`, would bake a silent type choice into every generic call, exactly the kind of implicit decision the language avoids elsewhere.

The price is one explicit `Int(...)` wrap at each such call site — paid once, per literal, in exchange for deleting the whole turbofish channel everywhere else.

---

## Size is part of the type (`Array<T, n>`)
**Spec:** [`generics.md`](../spec/generics.md) §7

The tempting shortcut is to leave an array's size out of its type — the stack pointer is just a register, an array never resizes after construction, and C99 VLAs already do this for locals, so there is prior art for "a runtime-sized local is fine." We bake the size in anyway, because the real cost of a runtime-sized type is not stack allocation but the loss of uniform stride. If two values of one type can differ in size, then `arr[i]` can no longer be `base + i * stride` with a constant stride; embedding the type in a struct leaves the outer layout unknown; copying needs a runtime size query; and calling conventions, which assume fixed-size parameters, break. Worse, the break propagates — an array of variable-size structs loses uniform stride, a struct containing one loses it, and so on up every containment chain. So `Array<T, n>` is the mechanism that guarantees every value of a given type is the same number of bytes, and that guarantee is precisely what makes indexing, copying, embedding, and calling cheap. It is the clearest case in the whole language of the general bargain: a high-level convenience is forbidden so that a low-level guarantee can hold, and the strictness is not a tax paid alongside the performance — it *is* the performance.

This rule is cited from [`adt.md`](../spec/adt.md) §4, where a directly inline self-referential type would have infinite size, which uniform stride forbids, which is why recursive types must box through `&`. The one loose end it leaves is that two arrays of different lengths are different types, so arithmetic on the size in a type position (`Array<T, rows * cols>`) becomes a type-identity question — and that is deferred, below.

---

## `Array<T, n>` is the single storage primitive
**Spec:** [`generics.md`](../spec/generics.md) §8

One compiler-provided fixed-size base case — `n` contiguous `T` — keeps the compiler's layout responsibility minimal. Every other fixed-size container, vectors and matrices among them, is defined in terms of `Array` and needs no extra compiler support, and dynamic containers, when they are specified, will be separate runtime-managed wrappers over opaque storage rather than extensions of `Array`. Blessing several container primitives instead would hand the compiler more layout surface to own; one base case plus library composition is smaller, and more honest about what is actually primitive.

---

## Deferred: what the model promises but does not yet deliver
**Spec:** [`generics.md`](../spec/generics.md) §9

This is the honest record of the gaps. The "types are executed functions" framing is elegant, and part of that elegance is a promise the current spec does not fully keep, so it is worth naming what is still open.

The biggest is type-level arithmetic on number parameters, `Array<T, rows * cols>`. The model says a type is a function you run at compile time, so running `*` on two numbers in a type position should fall out for free — except that two type expressions being "the same type" needs a type-level equality rule that canonicalizes such expressions, and that is exactly the hard part the framing papers over. It is why const-generic arithmetic took years elsewhere, and it is deferred pending that equality rule; the elegant framing oversells how much is free, and this is where the bill comes due.

Close behind is constraints on type parameters. `add(x T Type, y T Type)` silently assumes `T` supports `+`, but nothing yet lets an author require that `T` is addable, comparable, or printable, nor pins down how the compiler checks it. As specified the check is structural-at-instantiation, the way C++ templates worked before concepts, so a missing capability surfaces deep in the callee's body rather than at the signature. Bounded polymorphism is half of what makes generics usable in practice, and it is the most important thing to design next — most likely growing out of the concept machinery rather than a new sublanguage, since concepts are already how we type compile-time parameters.

Two smaller gaps round it out. A generic function *value*, one polymorphic over its own type or number parameters, is unspecified; the open question there is runtime representation — monomorphization versus dictionary passing — which is a memory-model decision rather than an overload-resolution one, since a generic function type is still a unique parameter shape (see [`functions.md`](../spec/functions.md) §7.6). And phantom type parameters — an introduced parameter with no path from any value argument, receiver, or literal that fixes it — currently have no way to be supplied at all, which is the visible edge of the no-`<>`-at-calls decision above. Named lane access (`.x`/`.y`/`.z`/`.w`) and element-access bounds-checking are also deferred, but on their own merits, not for any reason rooted in the model.
