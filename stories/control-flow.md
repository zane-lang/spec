# Stories: Control Flow

> **See also:** [`spec/control-flow.md`](../spec/control-flow.md) — the rules these chapters explain.

## An exit that opens no scope of its own

Branching was never the hard part. `if`, `elif`, and `else` are what they are everywhere, and the only choice worth recording is the small one: a single continuation keyword, `elif`, so that a chain of conditions reads as one connected thing rather than a staircase of nested `else if`. The pressure that actually shaped this document came from somewhere less obvious. Zane leans on lexical scopes more heavily than most languages — they carry hosting, destruction, and lifetime — and a language that leans on scopes needs a clean way to *leave* one.

The trouble surfaced the moment we tried to write a conditional early exit. Take the most ordinary intent there is — walk a loop, and bail out when some condition trips:

```zane
loop i to 1000 {
    if i < 100 {
        exit
    }
}
```

An exit, on its own, leaves the scope it sits in. But `if` is *itself* a scope, so the exit above leaves the `if` and nothing more — the loop keeps running. This is not a quirk of the example; it is the structural fact any exit-when-condition has to reckon with. To leave the *loop* conditionally, the condition cannot be tested inside a nested block, because the nested block is a scope in its own right and swallows the exit meant for the scope outside it. The test and the exit have to be a single construct that introduces no scope of its own.

That construct is `guard`. `guard condition` tests the condition and, when it holds, exits the *enclosing* scope directly — the loop, the function body, the plain block — without ever opening an intermediate scope for the exit to get trapped in ([`control-flow.md` §3.1](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#31-guard-exits-when-its-condition-is-true)):

```zane
loop i to 1000 {
    guard i < 100
}
```

This is the whole reason `guard` exists, and it decides the one thing about it that surprises newcomers — its polarity. A reader arriving from Swift knows `guard` as a construct that *verifies a precondition and bails when it fails*: it continues when its condition is true. Zane's `guard` is the opposite. It fires — it *leaves* — when its condition is true, because it is an **active exit** rather than a check. The condition names the circumstance under which you want to be gone, and being gone is what happens when the circumstance is real. Reading it as "verify this holds" is reading it backwards; reading it as "leave when this holds" is reading it as what it is. Someone coming from Swift has exactly one fact to relearn, and against a keyword that reads naturally once the "this is how you leave" framing is in hand, we judged that a small tax.

The one embellishment is the optional attached block: `guard condition { ... }` runs the block first and then exits ([§3.2](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#32-guard-may-run-a-pre-exit-block)). Nothing about it is load-bearing — the same effect is the block's statements followed by a bare `guard` — but it lets the work that must happen *on the way out* (a final log line, a last result, a cleanup call) sit visibly with the exit that occasions it, instead of drifting up above the condition that triggers it. That co-location was worth a little extra surface. It is a neat extension, not a necessity, and we added it on exactly those terms.

## Doing without `while`

With a clean in-scope exit in hand, a larger question came due: what does repetition look like, and does Zane want the loop every other language ships — `while`? We did not, and the discomfort predates the language. A `while` loop scatters the machinery of one repetition across three places. The state it turns on is set up before the loop; the condition that governs it lives in the header; the step that moves the state toward that condition sits at the bottom of the body. Reading one means holding all three at once and reassembling them in your head:

```
connection Connection()
tries = 0
tries = tries + 1
while connection is down {
    log(connection ping)
    tries = tries + 1
}
```

The same intent, written as a plain loop with the check stated *inside* it where it actually runs, reads in the order it executes — repeatedly: step, test-and-maybe-leave, work:

```
connection Connection()
loop forever {
    tries = tries + 1
    when connection is up, exit
    log(connection ping)
}
```

This is precisely the shape the previous chapter's `guard` was built to express, and that is no coincidence: the in-body exit and the dislike of `while` are two views of one preference. A repetition should show its moving parts in the order they run, not spread them across a header you have to read out of sequence.

But a loop that runs until something inside it decides to leave carries a risk we were not willing to ship as the default: nothing on the page bounds it. A loop whose only brake is a runtime condition can, if that condition never trips, run forever, and there is no written fact a reader — or the compiler — can point to that says otherwise. So Zane's `loop` always carries a written upper bound, and there is no unbounded form ([`control-flow.md` §4.3](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#43-there-is-no-dedicated-while)). The retry above becomes a counted loop whose `guard` still does the real stopping, with the ceiling now visible in the source:

```zane
connection Connection()
loop attempt from 1 to maxTries {
    guard connection:up()
    log(connection:ping())
}
```

The road we did not take was to keep `while` as a convenience and merely discourage it, or to offer a bare unbounded `loop` beside the counted one. We turned both down for the reason the [captured-intent bet](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/stories/foundations.md#the-bet-on-captured-intent) turns down other conveniences: a form that is available is a form that gets used, and an unbounded loop hides exactly the fact we wanted every loop to surface — *will this end?* The honest objection is that some computations genuinely are indefinite: a server running until shutdown, a poll with no fixed ceiling. Our answer is that indefinite repetition is not really a *loop* in the counted sense at all — it is scheduled work, and should use a scheduler or timer facility that names it as such. We expect that facility to look something like `setInterval`, whatever name its eventual API takes, making the recurrence and its scheduling policy explicit instead of smuggling both inside `while true`. Zane would rather make you spell the unbounded case out as the special thing it is than let it borrow the syntax of the ordinary bounded one.

The cost is real, and it lands on the loop that will certainly end but whose natural stopping condition is a test rather than a count. That loop must be given a bound anyway — a ceiling picked as a safe over-estimate, with a `guard` inside doing the actual stopping. The upper number is then sometimes a fiction: a figure large enough never to be reached in practice, written only to satisfy the rule that a loop must show a bound. We accept the awkwardness because the alternative — a loop with no written ceiling at all — is the precise thing we set out to remove.

## Counting from one

Loops forced a question we had managed to avoid until they existed: where does counting start? The instant you write the simplest possible counted loop, the answer stops being a matter of taste and becomes a matter of which intuition you are willing to break.

```zane
loop i to 3 {
    print(i)
}
```

Read it the way anyone would — *loop to three* — and it should run three times. Now try to honor that under zero-based counting and watch it fail to add up. If `i` starts at `0`, then either the loop runs while `i` is `0, 1, 2, 3`, which is four iterations, one more than "to three" promised; or it runs while `i` is `0, 1, 2` and stops with `i` never reaching the `3` the reader wrote down. Neither is satisfying — one breaks the count, the other breaks the correspondence between the bound you wrote and the last value you see. Zero-based counting simply cannot make `loop i to 3` both run three times *and* end at `3`.

One-based counting makes both true at once. `i` runs `1, 2, 3`: three iterations, ending on the very bound the source names ([`control-flow.md` §4.1](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#41-loop-from--to--is-inclusive)). That single clean case is what settled it, and once we looked, the advantage generalized past loops. Under one-based counting a positional index *is* a count: position `k` is the `k`-th element, and the number of elements up to and including it is exactly `k`. The last valid position of a sequence is therefore its size, not its size minus one ([§5.1](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#51-positional-indexing-is-1-based)) — `list[list:size()]` is the final element, with no correction. The index and the count are the same number everywhere, and the `- 1` that zero-based systems sprinkle through every boundary calculation is gone.

The road not taken is the one nearly every systems language walks, and we should be clear-eyed that we are the outlier here. Zero-based indexing is not an arbitrary tradition; it is the arithmetic of the machine. An element's address is `base + i * stride`, and that formula wants `i` to be a zero-based *offset* — the distance from the start — not a one-based *ordinal*, the position in a count. Choosing one-based means the surface index and the machine offset stop being the same number: somewhere underneath, one is subtracted to cross from the position the programmer wrote to the offset the hardware needs.

We were willing to pay that because of what Zane is, and is not. The language is fast, but it does not buy its speed by *pretending to be low-level* — it buys it by [trading low-level control for information](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/stories/foundations.md#strictness-is-the-performance-model), handing the compiler enough guaranteed structure to generate good code rather than handing the programmer raw offsets and hoping. In a language whose whole stance is that source captures *intent* rather than machine mechanism, the number in `list[k]` should be the ordinal a person means — *the k-th thing* — and the offset arithmetic underneath is exactly the sort of machine detail the compiler exists to absorb. The `- 1` at the boundary is the compiler doing its job, not a tax the programmer pays.

The cost is not only that hidden subtraction. It is friction at every seam with the zero-based world: data laid out by other systems, wire formats, algorithms transcribed from zero-based pseudocode, all of which count from zero and now need a translation — mental or actual — crossing into Zane. A programmer who has spent a career with `0..n-1` has a real habit to unlearn, and an off-by-one that a zero-based language made impossible is now possible in the other direction. We judged the intuitive win worth the standing friction: that `loop i to n` runs `n` times, that `list[list:size()]` is the last element, that the index and the count are finally one number. (We fixed only the ordinal base here; what happens when an index falls *outside* `1..size` is a separate question, left to the sections that own bounds behavior.)

## Control flow speaks in language types

The first versions of these chapters could say a condition "is `Bool`" and a loop variable "is `Int`" without confronting where those types came from. Once `core` was made an ordinary package, that omission became a contradiction: an ordinary dependency may be absent, renamed, or independently versioned, while `if` and `loop` still need stable answers before any library contract can help them. We tried to follow the package claim to its logical surface — `if Bool(true)._v`, `loop i to Int(20)._v` — and the result exposed why it was wrong. Control flow would depend on private wrapper layout, and every useful `Bool` or `Int` returned by an API would have to be torn open before the language could understand it.

So control flow speaks in the semantic language types themselves. A condition supplies a slot whose expected type is `Bool`; loop bounds supply slots whose expected type is `Int`; the loop variable is exactly `Int`. The same one-step implicit-constructor mechanism used at calls adapts an expression to those slots. This is coercion, not truthiness: no integer, string, reference, or collection becomes true merely because a runtime convention says so. A type enters a condition only when it is already `Bool` or deliberately declares a single applicable conversion to `Bool`, and the corresponding rule holds for loop bounds and `Int`.

The alternative was to require exact types and give literals a separate compiler-only exception. That would make `if true` and `loop i to 20` work, but it would create two conversion mechanisms — ordinary implicit constructors at calls and special literal lowering in control flow — precisely where one contract-shaped mechanism covers both. Treating language-defined expected types as coercion sites keeps the model local and lets safe, deliberate conversions participate without adding broad truthiness. The cost is that adding an implicit conversion can change whether an expression is accepted by control flow, the same cost it already carries at a call; coherence and the no-chaining rule keep that reach bounded.

## The last thing still tied to a package

Everything above treats `if` and `loop` as parts of the language, and for a long time nothing pressed on that. What pressed on it in the end was not elegance but versioning. We had already decided that `std` is not special and that every library is fetched, pinned, and remapped like any other dependency — and then `core`, the package that declares `Int` and `Bool`, sat outside that system as compiler infrastructure. The reason it sat there was `if`. A condition had to be a `Bool`, a loop bound had to be an `Int`, and those types therefore had to exist before any package could be resolved. The language named them, so the language owned them, so `core` could not be an ordinary dependency.

That is a real cost once you look at what side-by-side versioning is for. Two versions of one package coexist in a program under version-prefixed symbols ([`dependencies.md` §6](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/dependencies.md#6-symbol-versioning)), which is what lets a dependency graph hold a version soup instead of forcing every consumer onto one release. A language-level `if` cannot participate in that: given `core-1$Bool` and `core-2$Bool` in one build, the grammar has to pick one, and whichever it picks makes the other a second-class boolean. Type-level control flow is not merely a tidier way to arrange the same language — it is the thing that makes `core` versionable at all, because a conditional that is a method on a `Bool` needs no answer to "which `Bool`": each one brings its own.

So we moved branching and repetition out of the grammar and into `core` ([`control-flow.md` §3](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#3-branching-and-repetition)). `if(condition) { ... }` is a call that returns whether it ran; `ran!elif(...)` continues the chain and `ran:else()` ends it. The road we did not take was to keep `if` and `loop` as fixed sugar for those calls, the way `-` is fixed sugar for `a + ~b`. It would have preserved the familiar surface and cost nothing at the type level, and we turned it down because sugar in the grammar is still grammar: the keyword would have had to name a `Bool`, and we would have been back where we started with a nicer spelling.

The cost is legible and we should not talk around it. `if x { }` is not Zane any more, and a chain of conditions is now a sequence of independent calls threaded through a named value rather than one construct the compiler sees whole. Nothing checks that a chain is well-formed: an `else` with no `if` before it is just a method call on a `Bool`, and reassigning the chain value mid-chain is legal and quietly changes which branch runs. We accepted that because the alternative was accepting it in the versioning model instead, where it would have been permanent.

## A block is not a lambda

Moving `if` into a package immediately raised the question it had been hiding: what does a call take, such that the thing it takes is a *body*? A lambda is the obvious answer and the wrong one. Lambdas are values with complete written types, and they are explicitly forbidden to capture — a rule we had made on purpose, because a function value can be stored and run later, and captured state is then reachable from a frame nobody is looking at. A conditional whose body could not see the variables around it would be useless.

So a block argument is a different construct, and the difference is that it is not a value at all ([`control-flow.md` §2](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#2-block-arguments)). It has no written type, it cannot be stored, returned, bound to a symbol, or put in a field. `@concepts$Block` sits in the namespace we already had for source constructs that are not storage, next to the concept types that carry literals. Once a thing cannot outlive the call it is written at, the argument against capture evaporates: there is no later, and no other frame. The block reads and writes its surroundings exactly as a braced block written inline would, because after lowering that is what it is.

The harder question was what an exit inside a block means, and we got it wrong before we got it right. A block is a scope — it owns its declarations and destroys them — so the obvious reading is that `guard` inside a block leaves the block. That reading turns a `guard` in a repetition body into a *continue*, which quietly repeals the whole of [doing without `while`](#doing-without-while): the counted loop with a `guard` inside it stops being the way to write a conditional stop. So a block is a scope for bindings and transparent for control transfer, and `return`, `abort`, and `guard` inside one act on the scope containing the call.

That is a claim about lowering, not a mechanism for leaving a frame, and it took us a further pass to say so. A verb that takes a block is expanded at its call site, transitively through any call it passes the block on to, so a block never crosses a call boundary at run time and an exit inside one is a jump within a single frame. Nothing new is restricted by that: a block already had no type, no value form, and no way to escape, so no other frame could ever have held one. What it does mean is that a block-taking verb cannot be reached indirectly — which was already true, since callables have no value form.

## Where the capture would have bitten

Capture is safe because a block cannot outlive its call, and there is exactly one construct in the language that would have made that false. `spawn` runs a call concurrently, and a spawned block would have carried its captured state into a parallel task — the precise thing the no-capture rule was protecting against.

It is worth being clear about how badly it would have broken, because the obvious framing understates it. The visible danger is two spawned blocks mutating the same object, and that is real. But a *single* spawned block is already enough: the rule that keeps concurrent mutation safe reads the subject of the spawned call, and a mutation inside a block is not in that signature at all. One `spawn` of a block that mutates a reference-typed object would have falsified the guarantee that reference types are never mutated by spawned work — and that guarantee is what makes every concurrent *read* of the reference graph safe by construction.

We spent a while on a version that allowed it. Mutation is syntactically marked in Zane, so a compiler could sweep a block for `!` calls, resolve each subject to its root symbol, and refuse the ones that reach outside the spawn — no branch proving anywhere, and the machinery for resolving a place to its owner already exists. That analysis works, and we could have had it. What decided against it was that the sweep detects *that* a mutation happens, while the hard question is *which location* it hits — and the existing check is syntactic only because a prior rule has already forced every spawned mutable subject to be value-typed and therefore alias-free. Allowing captured reference objects under `spawn` takes that precondition away, and what replaces it is whole-program alias analysis, which we had explicitly declined to need.

The rule we took instead is one line: a verb that declares a block parameter may not be spawned ([`concurrency.md` §3.1](https://github.com/zane-lang/spec/blob/934d657a75da1d2403ad84367eaa4ff310c5f421/spec/concurrency.md#31-spawn-targets-function-and-method-calls-only)). It costs the ability to write `spawn withRetry(3) { work() }` — you wrap it in a named verb and spawn that instead — and in exchange the capture question never arises, the sweep is not needed, and the old prohibition on spawning a conditional survives the move into `core` without being restated as a special case.

## Two intrinsics, and what they are stated over

A package cannot branch by writing `if`, because `if` is what it is implementing. Something under `core` has to actually choose and actually repeat, and the shape of that something is where the whole separation is won or lost.

Two operations are enough. `@controlflow$branch` runs a block when its condition holds and does nothing otherwise; `@controlflow$repeat` runs a block a given number of times ([`control-flow.md` §5.1](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#51-two-intrinsics-stated-over-storage-primitives)). `branch` takes no fallback block, because the fallback is `branch` on the complement — an `else` is a branch on `~ran` — and a second parameter would have bought nothing but a second thing to specify. Nothing needs to be passed *into* a block, either, which is what lets a block have no parameters: a counted repetition advances the caller's own `Int` and the block sees it by capture, so no binding is introduced by the call at all.

The decision that matters, though, is what the intrinsics take. Stated over `Bool` and `Int` they would have depended on `core`, and the whole exercise would have been circular — control flow separated from the language only to be re-tied to a package one level down. So they are stated over storage primitives, which belong to no package. That is the hinge: an intrinsic names nothing that a dependency could rename or version, so `core` becomes an ordinary consumer of them rather than a privileged part of the compiler, and any package may declare control flow on exactly the same footing. A caller still writes ordinary values, because an intrinsic is called like a function and its arguments are coercion sites; the conversion from `Bool` to its primitive is an implicit constructor `core` declares, no different in kind from the one that turns `20` into an `Int`.

The bounded-loop rule survives this, and survives it better than it did as a keyword. `repeat` takes a count, so a single invocation always terminates and no construct built on it can omit a bound, whoever writes it; recursion stays the one unbounded path, as it always was. The guarantee stopped being a property of a construct the language happened to provide and became a property of the only shape available — which means "you can write your own control flow" and "every loop shows its ceiling" are true at the same time, with nothing enforcing the second but the first's vocabulary. A `while` is still writable, and still costs a written over-estimate with a `guard` inside it, exactly as [doing without `while`](#doing-without-while) already described.

## The exit that could not become a call

One construct did not move, and the reason is structural rather than a matter of taste. A call is an expression evaluated *inside* a scope. Whatever it does, it does before control returns to the statement that made it — so a call can never be the thing that leaves the scope it sits in. `guard` stays grammar because there is no way for it not to ([`control-flow.md` §4.3](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#43-guard-is-grammar-because-an-exit-cannot-be-a-call)).

We checked whether it could be dissolved a different way. With blocks transparent to control transfer, a bare condition-less exit written inside an `if` block would leave the scope containing that `if` — which looks like `guard` reassembled from smaller parts. It is not: it lands one level shallower than a `guard` written at the same depth, because the exit sees the `if` call as its own enclosing call. Reproducing `guard` would need the exit at the guard's depth, which needs the condition, which is where we started. The [first chapter](#an-exit-that-opens-no-scope-of-its-own)'s argument turns out to hold in a language where its original motivating example no longer parses.

We also declined an intrinsic for it. A `@controlflow$break` would have to name a scope further up and unwind to it, and Zane does not unwind — the error model exists in the shape it does precisely to keep runtime control flow out of failure handling. An exit intrinsic would have made that claim false for the sake of a construct that is already three words of grammar.

That left one loose end, and it was the sharpest question anyone asked about this design: if `guard` is grammar, what type is its condition? Answering "`Bool`" would have quietly undone the chapter above — the one construct the language kept would name a type belonging to a package, and under side-by-side versioning there is no single answer to which `Bool` that is. So a `guard` condition coerces to `@primitives$Bool` ([`control-flow.md` §4.1](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#41-guard-exits-when-its-condition-is-true)), and `core` bridges from its own `Bool` with an ordinary implicit constructor. No compiler-only lowering rule appears anywhere; it is the same coercion machinery that carries a literal. And it generalizes for nothing: any type that declares a conversion to that primitive is usable in a `guard`, so the construct is not even tied to `core`'s notion of truth — while the absence of general truthiness is preserved word for word, because a conversion still has to be declared deliberately and still cannot chain.

With that, the language's entire control-flow surface — block arguments, two intrinsics, and `guard` — names no declaration in any package. That was the point of the exercise, and it is also the precondition for `core` becoming an ordinary dependency, which is [its own story](dependencies.md#the-package-that-was-the-language).

## The exit that took no condition

The chapter above ends on a sentence that was true and complacent: the control-flow surface names no declaration in any package. It does not, but it was not free, and the bill arrived as a question about somebody else's `Bool`.

Count the implicit constructors a package needs to write a bool of its own and have it work everywhere a condition is accepted. One to `core`'s `Bool`, because `if` and `elif` are `core` verbs and their conditions are ordinary arguments. And one more to `@primitives$Bool`, because `guard` is grammar and grammar may not name a package's type. Two conversions, to two different destinations, for one idea — and the second exists for no reason a user of the language could be told, only for a reason about where the construct is implemented. We had moved the dependency out of `guard` without noticing that we had moved it onto everyone who wanted to be a condition.

The way out is the one [the previous chapter](#the-exit-that-could-not-become-a-call) had already examined and rejected: a bare, condition-less exit, with the condition supplied by whatever branch it sits in. The rejection turned on a claim about depth — that such an exit "lands one level shallower than a `guard` written at the same depth, because the exit sees the `if` call as its own enclosing call." That claim is false, and what refutes it is a rule from the same round — written down in [a block is not a lambda](#a-block-is-not-a-lambda) — that we simply never held up against it. A verb that declares a block parameter is expanded at its call site. `if` is such a verb. It has no frame for the exit to land on, and neither does any construct built the same way. The exit passes straight through and ends the enclosing verb — the exact depth the keyword reached. `guard` really was reassembled from smaller parts; we had written down the reason it worked before we wrote down the argument that it could not.

So the exit is [`@controlflow$exitFromCall()`](https://github.com/zane-lang/spec/blob/e4ceb3e8b9735fa712d952e3954a5d8490a18d05/spec/control-flow.md#42-exitfromcall-ends-the-innermost-invocation-that-has-a-frame), and it takes nothing. Not a condition, and not a block either — `guard c { ... }` was a pre-exit body, and a branch already has one. Every argument the construct used to take is now somebody else's parameter, which is the only way an exit can avoid naming a type: by having nothing to name one for. A package's bool needs one conversion again, to `core`'s `Bool`, and it works in conditions and exits alike because those are now the same position.

It is worth being exact about what this is not, because the earlier refusal of an exit intrinsic was right about the thing it refused. We would not add a `break`, because a break has to name a scope further up and unwind to it, and the error model exists in the shape it does to keep unwinding out of the language. `exitFromCall` names nothing. It ends the invocation containing it, which is decided entirely by a lowering rule that was already load-bearing for something else. The cost is that it is not a break, and [Zane still has none](https://github.com/zane-lang/spec/blob/e4ceb3e8b9735fa712d952e3954a5d8490a18d05/spec/control-flow.md#35-there-is-no-unbounded-repetition): nothing ends a repetition and resumes after it. A search that has an answer to carry out `return`s it, which crosses blocks the same way; work that must follow a repetition goes after a call to the verb that performs it. We looked at this and decided a language with no `while` had already accepted the shape of that trade.

What we did not expect was to gain something. A keyword can only ever exit where it is written, so it cannot be handed to anyone; there is no way to write a construct of your own that exits its caller when a keyword is the only exit. An intrinsic in the body of a block-taking verb is in a frame-less body, so it ends the *caller* — which means a package can ship a scoped resource form, a retry form, an early-return-on-empty form, and have the exit land in the user's verb where it belongs. The construct that we had singled out as the one thing that could not move turned out to be the one that most wanted to.

Two smaller things fell out. The exit carries no value, so it is legal only where falling off the end would be, in a `Unit` verb; anything with a return type leaves early through `return`, which was always the construct for that. And the spec's own `firstNegative` example — a counted loop with a `guard` in it, in a verb returning `Int` — had been quietly broken since the day the transparency rule was written: the exit left the verb before its own `return`. Nobody caught it while the rule was stated in terms of scopes. Stated in terms of frames the contradiction is on the surface, which is most of the argument for stating it that way.

The [keyword list](https://github.com/zane-lang/spec/blob/e4ceb3e8b9735fa712d952e3954a5d8490a18d05/spec/syntax.md#72-control-flow-keywords) is now empty.
