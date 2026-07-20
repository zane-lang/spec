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

The road we did not take was to keep `while` as a convenience and merely discourage it, or to offer a bare unbounded `loop` beside the counted one. We turned both down for the reason the [captured-intent bet](https://github.com/zane-lang/spec/blob/6882e129f144607e300367684d640d1d79bc41f4/stories/foundations.md#the-bet-on-captured-intent) turns down other conveniences: a form that is available is a form that gets used, and an unbounded loop hides exactly the fact we wanted every loop to surface — *will this end?* The honest objection is that some computations genuinely are indefinite: a server looping until shutdown, a poll with no fixed ceiling. Our answer is that indefinite repetition is not really a *loop* in the counted sense at all — it is a different tool wearing a loop's clothes, closer to a scheduler or a timer than to iteration, and it should be named as that tool rather than smuggled in as a `while true`. Zane would rather make you spell the unbounded case out as the special thing it is than let it borrow the syntax of the ordinary bounded one.

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
