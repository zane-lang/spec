# Stories: Control Flow

> **See also:** [`spec/control-flow.md`](../spec/control-flow.md) — the rules these chapters explain.

## An exit that opens no scope of its own

Branching was never the hard part. `if`, `elif`, and `else` are what they are everywhere, and the only choice worth recording is the small one: a single continuation keyword, `elif`, so that a chain of conditions reads as one connected thing rather than a staircase of nested `else if`. The pressure that actually shaped this document came from somewhere less obvious. Zane leans on lexical scopes more heavily than most languages — they carry hosting, destruction, and lifetime — and a language that leans on scopes needs a clean way to *leave* one.

> [!NOTE]
> Superseded: `if`, `elif`, and `else` are no longer keywords; they are `core` verbs. See "[Moving `if` and `loop` into `core`](#moving-if-and-loop-into-core)".

The trouble surfaced the moment we tried to write a conditional early exit. Take the most ordinary intent there is — walk a loop, and bail out when some condition trips:

```zane
loop i to 1000 {
    if i < 100 {
        exit
    }
}
```

An exit, on its own, leaves the scope it sits in. But `if` is *itself* a scope, so the exit above leaves the `if` and nothing more — the loop keeps running. This is not a quirk of the example; it is the structural fact any exit-when-condition has to reckon with. To leave the *loop* conditionally, the condition cannot be tested inside a nested block, because the nested block is a scope in its own right and swallows the exit meant for the scope outside it. The test and the exit have to be a single construct that introduces no scope of its own.

> [!NOTE]
> Superseded: that an exit nested in an `if` cannot reach the loop around it, so a separate construct had to test and exit at once. An exit ends the run of the innermost block argument around its call, the `if` block included. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

That construct is `guard`. `guard condition` tests the condition and, when it holds, exits the *enclosing* scope directly — the loop, the function body, the plain block — without ever opening an intermediate scope for the exit to get trapped in ([`control-flow.md` §3.1](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#31-guard-exits-when-its-condition-is-true)):

> [!NOTE]
> Superseded: that `guard` exits the enclosing loop or function body. It ends one run of the block argument around its call, one pass of a loop, and a `guard` directly in a verb's body is an error. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

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

> [!NOTE]
> Superseded: that a `guard` inside a counted loop is how the loop stops. It ends one pass, and a loop stops by `return`ing from the verb. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

But a loop that runs until something inside it decides to leave carries a risk we were not willing to ship as the default: nothing on the page bounds it. A loop whose only brake is a runtime condition can, if that condition never trips, run forever, and there is no written fact a reader — or the compiler — can point to that says otherwise. So Zane's `loop` always carries a written upper bound, and there is no unbounded form ([`control-flow.md` §4.3](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/spec/control-flow.md#43-there-is-no-dedicated-while)). The retry above becomes a counted loop whose `guard` still does the real stopping, with the ceiling now visible in the source:

> [!NOTE]
> Superseded: Zane now has an unbounded form beside the counted loop, the worker, which runs a step until the step returns `false`. See "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

```zane
connection Connection()
loop attempt from 1 to maxTries {
    guard connection:up()
    log(connection:ping())
}
```

The road we did not take was to keep `while` as a convenience and merely discourage it, or to offer a bare unbounded `loop` beside the counted one. We turned both down for the reason the [captured-intent bet](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/stories/foundations.md#the-bet-on-captured-intent) turns down other conveniences: a form that is available is a form that gets used, and an unbounded loop hides exactly the fact we wanted every loop to surface — *will this end?* The honest objection is that some computations genuinely are indefinite: a server running until shutdown, a poll with no fixed ceiling. Our answer is that indefinite repetition is not really a *loop* in the counted sense at all — it is scheduled work, and should use a scheduler or timer facility that names it as such. We expect that facility to look something like `setInterval`, whatever name its eventual API takes, making the recurrence and its scheduling policy explicit instead of smuggling both inside `while true`. Zane would rather make you spell the unbounded case out as the special thing it is than let it borrow the syntax of the ordinary bounded one.

> [!NOTE]
> Superseded: indefinite repetition now has its facility in `core`: `work`, and `setInterval`, which runs a step each time a `Duration` timer is ready. See "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

The cost is real, and it lands on the loop that will certainly end but whose natural stopping condition is a test rather than a count. That loop must be given a bound anyway — a ceiling picked as a safe over-estimate, with a `guard` inside doing the actual stopping. The upper number is then sometimes a fiction: a figure large enough never to be reached in practice, written only to satisfy the rule that a loop must show a bound. We accept the awkwardness because the alternative — a loop with no written ceiling at all — is the precise thing we set out to remove.

> [!NOTE]
> Superseded: a loop whose stopping condition is a test no longer needs a fictional ceiling; it is a worker, which names no bound. See "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

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

> [!NOTE]
> Superseded: `if` and `loop` are no longer language constructs whose slots the language types; branching and repetition are `core` verbs, and a condition is an ordinary argument. See "[Moving `if` and `loop` into `core`](#moving-if-and-loop-into-core)".

The alternative was to require exact types and give literals a separate compiler-only exception. That would make `if true` and `loop i to 20` work, but it would create two conversion mechanisms — ordinary implicit constructors at calls and special literal lowering in control flow — precisely where one contract-shaped mechanism covers both. Treating language-defined expected types as coercion sites keeps the model local and lets safe, deliberate conversions participate without adding broad truthiness. The cost is that adding an implicit conversion can change whether an expression is accepted by control flow, the same cost it already carries at a call; coherence and the no-chaining rule keep that reach bounded.

## Moving `if` and `loop` into `core`

Everything above treats `if` and `loop` as parts of the language, and for a long time nothing pressed on that. What pressed on it in the end was not elegance but versioning. We had already decided that `std` is not special and that every library is fetched, pinned, and remapped like any other dependency — and then `core`, the package that declares `Int` and `Bool`, sat outside that system as compiler infrastructure. The reason it sat there was `if`. A condition had to be a `Bool`, a loop bound had to be an `Int`, and those types therefore had to exist before any package could be resolved. The language named them, so the language owned them, so `core` could not be an ordinary dependency.

That is a real cost once you look at what side-by-side versioning is for. Two versions of one package coexist in a program under version-prefixed symbols ([`dependencies.md` §6](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/dependencies.md#6-symbol-versioning)), which is what lets a dependency graph hold a version soup instead of forcing every consumer onto one release. A language-level `if` cannot participate in that: given `core-1$Bool` and `core-2$Bool` in one build, the grammar has to pick one, and whichever it picks makes the other a second-class boolean. Type-level control flow is not merely a tidier way to arrange the same language — it is the thing that makes `core` versionable at all, because a conditional that is a method on a `Bool` needs no answer to "which `Bool`": each one brings its own.

So we moved branching and repetition out of the grammar and into `core` ([`control-flow.md` §3](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#3-branching-and-repetition)). `if(condition) { ... }` is a call that returns whether it ran; `ran!elif(...)` continues the chain and `ran:else()` ends it. The road we did not take was to keep `if` and `loop` as fixed sugar for those calls, the way `-` is fixed sugar for `a + ~b`. It would have preserved the familiar surface and cost nothing at the type level, and we turned it down because sugar in the grammar is still grammar: the keyword would have had to name a `Bool`, and we would have been back where we started with a nicer spelling.

The cost is legible and we should not talk around it. `if x { }` is not Zane any more, and a chain of conditions is now a sequence of independent calls threaded through a named value rather than one construct the compiler sees whole. Nothing checks that a chain is well-formed: an `else` with no `if` before it is just a method call on a `Bool`, and reassigning the chain value mid-chain is legal and quietly changes which branch runs. We accepted that because the alternative was accepting it in the versioning model instead, where it would have been permanent.

## A block is not a lambda

Moving `if` into a package immediately raised the question it had been hiding: what does a call take, such that the thing it takes is a *body*? A lambda is the obvious answer and the wrong one. Lambdas are values with complete written types, and they are explicitly forbidden to capture — a rule we had made on purpose, because a function value can be stored and run later, and captured state is then reachable from a frame nobody is looking at. A conditional whose body could not see the variables around it would be useless.

So a block argument is a different construct, and the difference is that it is not a value at all ([`control-flow.md` §2](https://github.com/zane-lang/spec/blob/0aae01df1c8d3480805b5c4d8353f6f643a3f525/spec/control-flow.md#2-block-arguments)). It has no written type, it cannot be stored, returned, bound to a symbol, or put in a field. `@concepts$Block` sits in the namespace we already had for source constructs that are not storage, next to the concept types that carry literals. Once a thing cannot outlive the call it is written at, the argument against capture evaporates: there is no later, and no other frame. The block reads and writes its surroundings exactly as a braced block written inline would, because after lowering that is what it is.

The harder question was what an exit inside a block means, and we got it wrong before we got it right. A block is a scope — it owns its declarations and destroys them — so the obvious reading is that `guard` inside a block leaves the block. That reading turns a `guard` in a repetition body into a *continue*, which quietly repeals the whole of [doing without `while`](#doing-without-while): the counted loop with a `guard` inside it stops being the way to write a conditional stop. So a block is a scope for bindings and transparent for control transfer, and `return`, `abort`, and `guard` inside one act on the scope containing the call.

> [!NOTE]
> Superseded: that a block is transparent to an exit. A `guard` inside a block ends that block's run; `return` and `abort` stay transparent. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

That is a claim about lowering, not a mechanism for leaving a frame, and it took us a further pass to say so. A verb that takes a block is expanded at its call site, transitively through any call it passes the block on to, so a block never crosses a call boundary at run time and an exit inside one is a jump within a single frame. Nothing new is restricted by that: a block already had no type, no value form, and no way to escape, so no other frame could ever have held one. What it does mean is that a block-taking verb cannot be reached indirectly — which was already true, since callables have no value form.

## A verb with a block parameter cannot be spawned

Capture is safe because a block cannot outlive its call, and there is exactly one construct in the language that would have made that false. `spawn` runs a call concurrently, and a spawned block would have carried its captured state into a parallel task — the precise thing the no-capture rule was protecting against.

It is worth being clear about how badly it would have broken, because the obvious framing understates it. The visible danger is two spawned blocks mutating the same object, and that is real. But a *single* spawned block is already enough: the rule that keeps concurrent mutation safe reads the subject of the spawned call, and a mutation inside a block is not in that signature at all. One `spawn` of a block that mutates a reference-typed object would have falsified the guarantee that reference types are never mutated by spawned work — and that guarantee is what makes every concurrent *read* of the reference graph safe by construction.

We spent a while on a version that allowed it. Mutation is syntactically marked in Zane, so a compiler could sweep a block for `!` calls, resolve each subject to its root symbol, and refuse the ones that reach outside the spawn — no branch proving anywhere, and the machinery for resolving a place to its owner already exists. That analysis works, and we could have had it. What decided against it was that the sweep detects *that* a mutation happens, while the hard question is *which location* it hits — and the existing check is syntactic only because a prior rule has already forced every spawned mutable subject to be value-typed and therefore alias-free. Allowing captured reference objects under `spawn` takes that precondition away, and what replaces it is whole-program alias analysis, which we had explicitly declined to need.

The rule we took instead is one line: a verb that declares a block parameter may not be spawned ([`concurrency.md` §3.1](https://github.com/zane-lang/spec/blob/934d657a75da1d2403ad84367eaa4ff310c5f421/spec/concurrency.md#31-spawn-targets-function-and-method-calls-only)). It costs the ability to write `spawn withRetry(3) { work() }` — you wrap it in a named verb and spawn that instead — and in exchange the capture question never arises, the sweep is not needed, and the old prohibition on spawning a conditional survives the move into `core` without being restated as a special case.

## Two intrinsics, and what they are stated over

A package cannot branch by writing `if`, because `if` is what it is implementing. Something under `core` has to actually choose and actually repeat, and the shape of that something is where the whole separation is won or lost.

Two operations are enough. `@controlflow$branch` runs a block when its condition holds and does nothing otherwise; `@controlflow$repeat` runs a block a given number of times ([`control-flow.md` §4.1](https://github.com/zane-lang/spec/blob/e1745e4d479fd171aca9040f88061c60e0d1f0f1/spec/control-flow.md#41-three-intrinsics-stated-over-storage-primitives-or-over-nothing)). `branch` takes no fallback block, because the fallback is `branch` on the complement — an `else` is a branch on `~ran` — and a second parameter would have bought nothing but a second thing to specify. Nothing needs to be passed *into* a block, either, which is what lets a block have no parameters: a counted repetition advances the caller's own `Int` and the block sees it by capture, so no binding is introduced by the call at all.

> [!NOTE]
> Superseded: two intrinsics were not enough; a third, the exit, was added when `guard` stopped being grammar, and two more run workers. See "[`guard` stops being a keyword](#guard-stops-being-a-keyword)" and "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

The decision that matters, though, is what the intrinsics take. Stated over `Bool` and `Int` they would have depended on `core`, and the whole exercise would have been circular — control flow separated from the language only to be re-tied to a package one level down. So they are stated over storage primitives, which belong to no package. That is the hinge: an intrinsic names nothing that a dependency could rename or version, so `core` becomes an ordinary consumer of them rather than a privileged part of the compiler, and any package may declare control flow on exactly the same footing. A caller still writes ordinary values, because an intrinsic is called like a function and its arguments are coercion sites; the conversion from `Bool` to its primitive is an implicit constructor `core` declares, no different in kind from the one that turns `20` into an `Int`.

The bounded-loop rule survives this, and survives it better than it did as a keyword. `repeat` takes a count, so a single invocation always terminates and no construct built on it can omit a bound, whoever writes it; recursion stays the one unbounded path, as it always was. The guarantee stopped being a property of a construct the language happened to provide and became a property of the only shape available — which means "you can write your own control flow" and "every loop shows its ceiling" are true at the same time, with nothing enforcing the second but the first's vocabulary. A `while` is still writable, and still costs a written over-estimate with a `guard` inside it, exactly as [doing without `while`](#doing-without-while) already described.

> [!NOTE]
> Superseded: recursion is no longer the one unbounded path, and a `while` no longer has to be faked with an over-estimate; a worker is the other unbounded form. See "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

## `guard` stops being a keyword

One construct, `guard`, did not move, and for a while we had an argument for why it could not. A call is an expression evaluated *inside* a scope. Whatever it does, it does before control returns to the statement that made it — so a call can never be the thing that leaves the scope it sits in. `guard` stayed grammar, the single survivor of a redesign that had just emptied the grammar of everything else.

We checked whether it could be dissolved a different way. With blocks transparent to control transfer, a bare condition-less exit written inside an `if` block would leave the scope containing that `if` — which looks like `guard` reassembled from smaller parts. It seemed not to be: it lands one level shallower than a `guard` written at the same depth, because the exit sees the `if` call as its own enclosing call. Reproducing `guard` would need the exit at the guard's depth, which needs the condition, which is where we started.

A dedicated intrinsic for leaving a scope looked no better. A `@controlflow$break` would have to name a scope further up and unwind to it, and Zane does not unwind — the error model exists in the shape it does precisely to keep runtime control flow out of failure handling. An unwinding primitive would have made that claim false for the sake of a construct that is already three words of grammar.

So we kept the keyword, and it had a loose end that turned out to be the sharpest question anyone asked about this design: if `guard` is grammar, what type is its condition? Answering "`Bool`" would have quietly undone [the chapter above](#moving-if-and-loop-into-core) — the one construct the language kept would name a type belonging to a package, and under side-by-side versioning there is no single answer to which `Bool` that is. So a `guard` condition coerced to `@primitives$Bool`, and `core` bridged from its own `Bool` with an ordinary implicit constructor. No compiler-only lowering rule appeared anywhere; it was the same coercion machinery that carries a literal.

That is where we stopped, and the sentence we stopped on was true and complacent: the control-flow surface names no declaration in any package. It did not name one. But the surface is not the only place a package's `Bool` can be pinned down, and the bill arrived as a question about somebody else's.

Count the implicit constructors a package needs to write a bool of its own and have it work everywhere a condition is accepted. One to `core`'s `Bool`, because `if` and `elif` are `core` verbs and their conditions are ordinary arguments. And one more to `@primitives$Bool`, because `guard` was grammar and grammar may not name a package's type. Two conversions, to two different destinations, for one idea — and the second existed for no reason a user of the language could be told, only for a reason about where the construct happened to be implemented. We had moved the dependency out of `guard` without noticing that we had moved it onto everyone who wanted to be a condition.

Which sent us back to the escape we had rejected, and to the claim about depth we had rejected it on. The observation was right: a condition-less exit does land one level shallower than a `guard` written where it sits. The conclusion drawn from it was backwards. Landing one level out is not the defect. It is the whole mechanism, if you point it at the right level.

Make the intrinsic end *its caller's* invocation rather than its own, and the construct we could not build becomes four lines of ordinary Zane:

```zane
package core

Unit guard(condition Bool) {
    if(condition) {
        @controlflow$exitFromCall()
    }
    return Unit()
}
```

`guard(cond)` ends the verb that wrote it, exactly as the keyword did, because that verb is what called `guard`. The condition is now a parameter of a `core` verb, so it is `core`'s `Bool` like every other argument in the language, and the second implicit constructor is gone. `guard` did not have to be dissolved into smaller parts after all; it had to be moved one level down and re-declared, and what lets it be re-declared is the very "one level shallower" we had written off as the reason it could not be.

> [!NOTE]
> Superseded: that `guard(cond)` ends the verb that wrote it. It ends the run of the block its call is written in. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

[Two operations were enough](#two-intrinsics-and-what-they-are-stated-over) only for as long as one construct stayed grammar. A third is what the last keyword costs, and it is the cheapest of them: `branch` and `repeat` are stated over storage primitives, while an exit takes no arguments at all, so nothing whatsoever appears in its signature for a package to rename or version ([`control-flow.md` §4.1](https://github.com/zane-lang/spec/blob/e1745e4d479fd171aca9040f88061c60e0d1f0f1/spec/control-flow.md#41-three-intrinsics-stated-over-storage-primitives-or-over-nothing)).

That inverts what the intrinsic is *for*. It is not written where an exit is wanted — in a verb's own body it ends that verb's *caller*, which is a bug in anything that meant `return`. It is a tool for declaring exiting verbs, and `core`'s `guard` is the one every program uses. Any package can declare others: a `guardEmpty`, a `bail`, a domain-specific precondition check that leaves the caller. A keyword could never be handed to anyone, because a keyword only ever exits where it is written; [an exit that ends its caller](https://github.com/zane-lang/spec/blob/e1745e4d479fd171aca9040f88061c60e0d1f0f1/spec/control-flow.md#42-exitfromcall-ends-its-callers-invocation) is a thing you can put in a signature.

> [!NOTE]
> Superseded: that the intrinsic in a verb's body ends that verb's caller. It ends the run of the block the call to that verb is written in. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

Two rules had to move with it. The exit carries no value, so the invocation it ends has to return `Unit` — and because the caller is decided at the call site, that check belongs to the call rather than to the exiting verb's declaration; the same `guard` is fine in one caller and rejected in the next. And the reach is exactly one level: the exit ends whoever called the exiting verb and stops there, so in `outer` → `helper` → `guard` it is `helper` that ends and `outer` that runs on. Any further and the primitive would be the unwinding mechanism we had refused a `break` for.

> [!NOTE]
> Superseded: that the ended invocation must return `Unit` and that the exit reaches the invocation that called the exiting verb. The exit ends a block's run, so no `Unit` rule remains. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

That refusal still stands, and it is worth being exact about what this is still not. A break has to name a scope further up. Nothing here names a scope, which is why an exit could be added where a break could not, and also why it buys none of what a break would have. [Zane still has no break](https://github.com/zane-lang/spec/blob/e1745e4d479fd171aca9040f88061c60e0d1f0f1/spec/control-flow.md#35-there-is-no-unbounded-repetition): nothing ends a repetition and resumes after it, because a `guard` inside a loop body ends the verb the loop was written in, not the loop. A search that has an answer to carry out `return`s it, which crosses blocks the same way; work that must follow a repetition goes after a call to the verb that performs it. A language with no `while` had already accepted the shape of that trade.

> [!NOTE]
> Superseded: that a `guard` inside a loop body ends the verb the loop was written in. It ends one pass of the loop. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

One thing fell out that nobody was looking for. The spec's own `firstNegative` example — a counted loop with a `guard` in it, in a verb returning `Int` — had been quietly broken since the day the transparency rule was written: the exit left the verb before its own `return`, so an `Int` verb produced nothing. Nobody caught it while the rule was stated in terms of scopes. Stated in terms of which invocation is the caller, the contradiction is on the surface, which is most of the argument for stating it that way.

`guard` reads the same as it always did. It is [no longer a keyword](https://github.com/zane-lang/spec/blob/876f5c02202d623c5f36eebb9e7b664572c84ef1/spec/syntax.md#72-control-flow-keywords), and the keyword list is now empty. The whole control-flow surface — block arguments and three intrinsics — names no declaration in any package, and that is the precondition for `core` becoming an ordinary dependency, which is [its own story](dependencies.md#core-becomes-part-of-the-language-again).

> [!NOTE]
> Superseded: the surface has five intrinsics, the worker pair among them, and still names no declaration in any package. See "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

## No `exitFromCall` in a lambda body

[The exit that took no condition](#guard-stops-being-a-keyword) left one sentence standing that we had not looked at hard: a lambda has a frame of its own, so it is a caller like any other. That is true, and it is the harmless half. A lambda that calls `guard` is `guard`'s caller, and the exit ends the lambda, exactly as it ends any verb that writes it. The other half is what an `@controlflow$exitFromCall()` written in the lambda's *own* body does, and that half had no answer anyone could check.

> [!NOTE]
> Superseded: that a `guard` called by a lambda ends the lambda. A call to `guard` is legal only inside a block, and ends that block's run. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

It came up when the compiler first had to call through a function value. The two rules the exit brought with it are both decided at the call. The invocation an exit ends has to return `Unit`, and the check belongs to the call rather than to the exiting verb's declaration, which is what lets the same `guard` be fine in one caller and rejected in the next. A call to a declared verb can be checked that way because the call names the verb, and the verb's body says whether it exits. A call through a function value names a value. `f(x)` might be the lambda written two lines up, or a parameter, or a member someone reassigned, and any lambda of `f`'s type may sit behind it. A lambda carries its complete written type ([`functions.md` §7.2](https://github.com/zane-lang/spec/blob/1da001a68a83def195b1c0493299a0a183de764b/spec/functions.md#72-lambdas-are-self-typed-function-values)), and nothing in that type says whether calling it can end the caller. So an exit in a lambda's body would end an invocation the compiler had no way to check against the `Unit` rule, and no way to lower either, since the call does not know it has a third way to finish.

> [!NOTE]
> Superseded: the `Unit` rule this paragraph checks against. The lambda rule now rests on a function type not recording whether the function exits, so a call through one cannot be checked for sitting in a block. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

We saw three ways out. The first was to make exiting part of the function type, beside the abort type, so that `Unit[Int]` and an exiting `Unit[Int]` would be different types and every call would know. That is the honest version, and we did not want it. It adds a component to every function type for a feature almost no function value would use, and it makes a type's author think about exits every time they write one. The second was to assume the worst: treat every call through a function value as one that might exit, so the rules apply to all of them. That would have forbidden calling a function value from any verb that returns something other than `Unit`, which is most of the places a callback gets called. It was out as soon as it was said.

The third was to forbid it: the intrinsic may not appear in a lambda's body, including a block argument written there ([`control-flow.md` §4.2](https://github.com/zane-lang/spec/blob/1da001a68a83def195b1c0493299a0a183de764b/spec/control-flow.md#42-exitfromcall-ends-its-callers-invocation)). That is what we took, and it costs very little. The intrinsic was never something written where an exit is wanted; it is a tool for declaring exiting verbs, and a lambda is not a declaration anyone calls by name. A lambda that wants to stop early `return`s, and one that calls `guard` still stops itself, because it is `guard`'s caller. What is given up is an exiting function value, a callback that ends whoever calls it. We could not find a use for one that a declared exiting verb, or a return value the caller acts on, does not serve at least as well.

> [!NOTE]
> Superseded: that a lambda which calls `guard` stops itself. The `guard` ends the run of the block it is written in. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

The rule also keeps a claim from the previous chapter true without exceptions. The exit's check belongs to the call, and with this rule every call that can exit names a verb whose body says so.

## A block that yields nothing

The block argument came with a second form. `@concepts$Block<T>` was a block that yields a `T`, ending each yielding path with `resolve`, and its one use was a deferred condition: `ran!elif({ resolve expensiveCheck() }) { ... }` ran the check only when no earlier branch had. It read well, and nothing pressed on it until `core` came to declare that `elif` overload and could not.

The receiving verb had nothing to do with the block. A verb does not run a block itself; it hands it to an intrinsic, and [the two intrinsics that take one](#two-intrinsics-and-what-they-are-stated-over) take a block that yields nothing. `branch` runs its body when a condition holds, and `repeat` runs it a counted number of times, and neither has anywhere to put a value the body produces. So a `Block<Bool>` parameter could be passed on but never read, and the `elif` that declared one had no way to learn the condition it was meant to defer. The compiler had reached the same place from the other side: it never accepted a type argument on `@concepts$Block`, and listed the deferred condition as a form it had no way to build.

There were two ways out. One was a fourth intrinsic that runs a block and returns what it resolves to, which would have made `Block<T>` work as written. The other was to drop the form. We dropped it. A block is now only `@concepts$Block` and yields nothing ([`control-flow.md` §2.4](https://github.com/zane-lang/spec/blob/555bc1c4275c4292c5b73e20471f89ca79bae5ae/spec/control-flow.md#24-a-block-yields-nothing)). `resolve` became transparent to a block as `return` and `abort` already were: written in one, it finishes the abort handler around the call. A condition that must run only when reached is written inside the `else` of the chain so far, where it starts a chain of its own ([§3.3](https://github.com/zane-lang/spec/blob/555bc1c4275c4292c5b73e20471f89ca79bae5ae/spec/control-flow.md#33-a-condition-is-evaluated-before-the-call)). The intrinsics stay the three that every construct is built from, and a call's arguments are evaluated before the call in every case, conditions included.

> [!NOTE]
> Superseded: the intrinsics are no longer three; two that run workers take a function value rather than a block. See "[Workers replace the unbounded loop: `work` and `setInterval`](#workers-replace-the-unbounded-loop-work-and-setinterval)".

The cost falls on the chain. A deferred condition no longer continues the chain it belongs to: it opens a nested one one level deeper, and every branch after it moves in with it. A long run of expensive conditions now reads as a staircase where it used to read as a list. We accepted that because the list was never something a verb could build. The form existed only in the spec, and keeping it would have meant a new intrinsic whose only use was this one overload.

## Workers replace the unbounded loop: `work` and `setInterval`

[Doing without `while`](#doing-without-while) ended on a promise. An indefinite repetition is scheduled work, we said, and it should get a facility that names it, something like `setInterval`. Until that facility existed, a loop whose stopping condition was a test carried a ceiling and a `guard`, and we had called the ceiling "sometimes a fiction". In practice it was a fiction every time. We ported an interpreter to measure the compiler, and its fetch loop runs until the program counter passes the end of the program. There is nothing to count, so the loop was written `repeat(Int(9223372036854775807))` with an exit inside. That number is not an over-estimate of anything. It is the largest value an `Int` holds, which makes the loop a `while` with a number stapled to it. The rule that every loop shows its bound had become a rule that every loop shows a number.

So we went back to the promised facility, and the first question was what it should mean. We wanted three things from it. A reader should see it as a worker with state of its own, not as one more step of the code around it. It should obviously be something to `spawn` when it should not hold up its caller. And it should read as controlled execution rather than a spinning CPU, which is what the `while true` at the heart of a game or a server reads as.

The obvious form was a block: `work { ... }`, run until something inside it says stop. It would have read well, and it would have been `while` renamed. A block is expanded into the verb that calls it ([`control-flow.md` §2.3](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/control-flow.md#23-a-block-is-a-scope-for-bindings-not-for-control-transfer)), and it captures that verb's frame. Every local of the caller would be in reach, and the condition that ends the repetition could sit anywhere in the body, which is the scattering we had refused `while` for. A block-taking verb also cannot be spawned ([`concurrency.md` §3.1](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/concurrency.md#31-spawn-targets-function-and-method-calls-only)), and running beside its caller is what an indefinite repetition is most often for.

A function value has neither problem, and its restriction turned out to be the point. A lambda captures nothing ([`functions.md` §7.4](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/functions.md#74-lambdas-do-not-capture)), so a step sees only the subject it is handed. A worker therefore cannot turn on a handful of the caller's locals. Its state has to be gathered into one value, the worker's own, and that is exactly "a worker with state of its own". We did not have to add a rule to get the meaning. The existing rule forces it. So a **worker** is a subject and a **step**, a `Bool[this S] mut` function value, and `core`'s `subject!work(step)` runs the step until it returns `false` ([`control-flow.md` §3.7](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/control-flow.md#37-work-and-setinterval-run-a-worker)). Neither method takes a block, so `spawn machine!work(...)` runs a worker without blocking, and the rule that a spawned `mut` call needs a value-typed subject applies as it does to any other.

How the step says stop was settled by an earlier chapter. [A lambda's body may not contain `exitFromCall`](#no-exitfromcall-in-a-lambda-body), and a `guard` inside the step ends only that one call of it, because the lambda is the guard's caller. So the stop is the step's result: `true` to run again, `false` when the work is done. That keeps the stopping condition in the one place a reader looks for it, the step's `return`. The result converts to `@primitives$Bool` the way a condition does at `branch`, so a step returns `core`'s `Bool` as written.

> [!NOTE]
> Superseded: that a `guard` inside a step ends that call of it. A `guard` directly in the step's body is rejected, and one in a block inside the step ends that block's run. See "[An exit ends the run of a block](#an-exit-ends-the-run-of-a-block)".

`core` could have built `work` out of what it already had. A method that runs the step and calls itself again while the step returns `true` is four lines of recursion. It would have needed a tail-call guarantee the language does not make, and without one each step would cost a frame. We chose a dedicated intrinsic instead, whose lowering is a plain loop. `@controlflow$work` runs the step until it returns `false`, and `@controlflow$workEvery` also waits a given number of nanoseconds between steps ([`control-flow.md` §4.1](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/control-flow.md#41-five-intrinsics-stated-over-primitives-function-values-and-type-parameters)). `core` wraps them as `work` and `setInterval`, the second taking a `Duration`. The intrinsics take a subject of any type and a function value over it, so they still depend on no package, and the separation the [two original intrinsics](#two-intrinsics-and-what-they-are-stated-over) were built for holds for five.

> [!NOTE]
> Superseded: that `@controlflow$workEvery` waits between steps and that the worker intrinsics take their subject as an argument. `@controlflow$register` waits on an event source, `setInterval` wraps it, and both intrinsics are methods of their subject. See "[`register` replaces `workEvery`, and the worker intrinsics become methods](#register-replaces-workevery-and-the-worker-intrinsics-become-methods)".

The names were chosen as plainly. "Worker" says what the thing is, a unit of ongoing work with state of its own. "Loop" would have said the opposite, a step of the program it is written in. "Task" and "process" name scheduling machinery, and the language deliberately has none ([`concurrency.md` §5.4](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/concurrency.md#54-no-language-level-process-or-channel-abstraction)). `setInterval` is borrowed from the web, because a reader already knows it as "do this every so often until it is stopped". It differs from its namesake in two ways: it runs the first step at once, and it stops by its step's result rather than by a separate cancel call. The intrinsics are named for what they do, as `branch` and `repeat` are, so the waiting one is `workEvery`, which pairs with `work`.

> [!NOTE]
> Superseded: the waiting intrinsic's name, `workEvery`. It is `register`. See "[`register` replaces `workEvery`, and the worker intrinsics become methods](#register-replaces-workevery-and-the-worker-intrinsics-become-methods)".

The bounded-loop rule changed shape and survived. `repeat` is still the one loop, and every loop still shows its bound ([`control-flow.md` §4.3](https://github.com/zane-lang/spec/blob/75c600f6e937b933fc02f80105892ee0c9979b37/spec/control-flow.md#43-a-loop-is-bounded-by-the-shape-of-repeat-and-a-worker-names-its-recurrence)). What we gave up is the claim that recursion is the only unbounded path. A worker is another, and we accepted that because it names itself: an unbounded repetition is now a call to `work` or `setInterval`, written where it starts, with its stop in its step, instead of a ceiling nobody believes. Two costs remain. A worker cannot reach its caller's locals, so code that only wanted a quick unbounded pass over a few of them first gathers them into a type. And a step that never returns `false` runs for as long as the program does. Nothing in the language prevents that, but the call says plainly that it can happen.

The `core` wrappers asked for one more thing. A worker's subject is any type: an interpreter's machine, a game's world, a server's connection table. So `work` had to be a method on a subject of every type, which the language did not yet have. That is [its own chapter](generics.md#a-methods-subject-may-be-a-type-parameter).

## `register` replaces `workEvery`, and the worker intrinsics become methods

`@controlflow$workEvery` slept. Between one step and the next it suspended the thread it ran on, which was harmless while a spawned call that waited was parked and left its thread to other work. When [parking was dropped](concurrency.md#parking-is-dropped-a-wait-holds-back-only-its-caller), a sleeping thread became a thread that no other worker could use. Our first answer kept `workEvery` and ran it on the runtime's timer instead of a sleep. Then we noticed that a worker pool already had a way to wait for time: a worker is started with event sources, and a timer is one of them. A worker that runs every so often was a special case of a worker that runs when an event source is ready.

So the timed intrinsic became `register`. It takes an event source and a step, calls the step, and calls it again each time the source is ready, until the step returns `false` ([`control-flow.md` §3.7](https://github.com/zane-lang/spec/blob/d9f05254cd825235ad19254b3964c26b6233a862/spec/control-flow.md#37-work-and-register-run-a-worker)). It waits as a read of a spawned result waits: it holds back only its caller, so a `register` written without `spawn` returns only once its step returns `false`, and `spawn` runs it beside its caller. A timer from `EventSource.every` repeats what `workEvery` did, except that the timer counts each interval from the moment it was last ready rather than from the end of a step. With any other source the runtime can watch, the same call waits for something other than time.

The first form we wrote was a method on the event source, `EventSource.every(interval):register(step)`, which reads as "register this step with this source". It had nowhere to put the worker's state. A step writes its subject, and the subject of `register` would have been the source, so the state would have needed a parameter of its own. The state is the subject: it is what the step advances, and what `work` already ran on. So `register` is a method on the thing it mutates, `clock!register(source, step)`, and takes the source as an argument. `core`'s wrappers follow the same shape: `work`, `register`, and `setInterval` are all `mut` methods on a subject of any type, and `setInterval` is `register` on `EventSource.every(every)`.

We also asked whether `work` was just `register` on a source that is always ready, since a timer with an interval below one nanosecond is one. The two behave the same. But `work` has nothing to wait for, and routing the one form that never waits through the machinery for waiting would have bought nothing. So `work` stays a plain loop.

The intrinsics' own signatures had a second problem, which `register` made plain. `@controlflow$work(subject S Type, step ...)` was called like a function, and its first argument was the one thing it wrote. Nothing at the call showed the write. The spec needed a paragraph to explain that `subject` is passed as a `!` call's subject is, borrowed mutably, with every step running on that one borrow. Renaming the parameter `this` would not have fixed it, because a verb whose first parameter is `this` is a method, and a method is called on its subject. So the worker intrinsics became `mut` methods whose subject is a type parameter, `@controlflow$work(this S Type, step R Type[this S] mut) mut` ([§4.1](https://github.com/zane-lang/spec/blob/d9f05254cd825235ad19254b3964c26b6233a862/spec/control-flow.md#41-five-intrinsics-stated-over-primitives-function-values-and-type-parameters)), and a call names them with their namespace as the qualifier, as a qualified method call names its package: `this!@controlflow$work(step)` ([`functions.md` §6.2](https://github.com/zane-lang/spec/blob/d9f05254cd825235ad19254b3964c26b6233a862/spec/functions.md#62-qualified-method-calls)). The `!` shows the write where it happens, and the explaining paragraph shrank to the borrow rule every `!` call already has.

The name `workEvery` was chosen to pair with `work`, and nothing is left for it to pair with. `register` names the act itself: the call registers the step with an event source. The cost is a second shape for an intrinsic: `@controlflow$` now qualifies both function calls and method calls, and a reader meets `@controlflow$branch(...)` and `this!@controlflow$work(...)` in the same package.

## An exit ends the run of a block

[Making `guard` an ordinary verb](#guard-stops-being-a-keyword) had the exit end its caller's invocation, and that brought a rule with it: the invocation it ends must return `Unit`, because the exit carries no value. A second rule, written for a different reason, was already standing next to it. Every block-bodied verb returns explicitly on every path, `Unit` included ([`functions.md` §3.5](https://github.com/zane-lang/spec/blob/b158a77b47b03590543750f08ea46d8b04559197/spec/functions.md#35-block-bodied-verbs-return-explicitly)). Taken together, the two meant that an exit could end a body only by skipping the `return` that the body was required to end in, and only in the one return type where that skipped `return` had nothing to say.

The compiler read the exit the other way when it implemented it, and that reading is the one `guard` was meant to have from the start: a `guard` exits a block. A function has to return something, so an exit cannot end one. A block yields nothing ([the chapter on blocks that yield nothing](#a-block-that-yields-nothing)), so ending a run of one needs no value at all. An exiting verb that exits therefore ends the current run of the innermost block argument around its call, and the call is legal only inside a block argument ([`control-flow.md` §2.3](https://github.com/zane-lang/spec/blob/b158a77b47b03590543750f08ea46d8b04559197/spec/control-flow.md#23-a-block-is-transparent-to-return-and-abort-and-an-exit-ends-its-run)). `return` and `abort` stay transparent to blocks, as before; the exit is now the one transfer a block stops.

This is the reading [the chapter on blocks](#a-block-is-not-a-lambda) considered first and turned down, because it turns a `guard` in a repetition into a *continue*, so the counted loop with a `guard` inside it stops being the way to write a conditional stop. We take it now with that cost in view. A conditional stop is written `if(done) { return result; }` inside the loop, which leaves the verb and carries its value out through the blocks, and a `guard` skips the rest of one pass. Zane still has no *break*: nothing ends a repetition and resumes after it.

The rest moved with it. The `Unit` rule is gone, since a block's run gives nothing back to replace. The exit still reaches one level, but the level is now a block around the call rather than the invocation that made it ([`control-flow.md` §4.2](https://github.com/zane-lang/spec/blob/b158a77b47b03590543750f08ea46d8b04559197/spec/control-flow.md#42-exitfromcall-ends-the-run-of-its-callers-block)), so a `guard` written directly in a verb's body is an error, and a verb that wants to stop itself `return`s. The lambda rule stands, for a narrower reason. A function type does not record whether calling the function exits, so a call through a function value could not be checked for sitting in a block, and the intrinsic stays out of a lambda's frame. A `guard` written in a block inside a lambda ends that block's run, like anywhere else.
