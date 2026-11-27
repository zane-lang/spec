# Zane Control Flow

This document specifies how Zane branches and repeats. Neither is a language construct: both are ordinary calls declared by the `core` package over intrinsics. Branching and counted repetition take a **block argument**; a worker (`work`, `register`, `setInterval`) repeats a function value instead, and takes no block. What the language itself contributes is the block, the control-flow intrinsics, and the 1-based ordinal convention.

> **See also:** [`syntax.md`](syntax.md) §4.8 and §5 for the canonical surface syntax. [`operators.md`](operators.md) §2.4 for `Bool`'s `and` and `or`. [`types.md`](types.md) §2.6 for the fundamental types these calls are written in terms of.

---

## 1. Overview

Zane has no `if` statement, no `loop` statement, and no exit keyword. It has a mechanism to hand a run of statements to a verb, three control-flow intrinsics that a verb can use to run the handed statements conditionally, run them repeatedly, or end the run of the block argument around a call to the verb, and two that run a function value on a subject until it reports that its work is done. Everything a reader recognizes as control flow is built from those.

- **`Block arguments`.** A braced run of statements may be passed to a call. It captures its surroundings, cannot escape, and runs during the call.
- **`Branching, repetition, and exits are calls`.** `if`, `elif`, `else`, `guard`, and counted repetition are `core` declarations, resolved and overloaded like any other verb.
- **`Five control-flow intrinsics`.** `@controlflow$branch`, `@controlflow$repeat`, and `@controlflow$exitFromCall` act on blocks; `@controlflow$work` and `@controlflow$register` run a worker. They are the whole of the control flow the language supplies, and they are stated over storage primitives, function values, and type parameters, so none depends on a package.
- **`A loop is bounded, a worker names its recurrence`.** `repeat` takes a count, so no loop built on it repeats without a written bound. A repetition that runs until a condition holds is a **worker**: a function value run on a subject of its own until it returns `false`, which `core` declares as `work`, `register`, and `setInterval` and which may be spawned. Many workers that run the same step run in a worker pool.
- **`An exit ends a block's run`.** When an exiting verb such as `guard` exits, it ends the run of the innermost block argument around its call, one pass of a loop or one run of an `if` block. A call to one is legal only inside a block argument, since a block argument is the one construct that yields no value.
- **`No control-flow keywords`.** The exit intrinsic makes the verb holding it end its caller's block, which is what lets `core` declare `guard` as an ordinary verb; nothing in the language's control-flow surface names a type.
- **`1-based ordinals`.** Counted repetition and positional indexing start at `1`, not `0`, and a position outside its sequence stops the program.

---

## 2. Block Arguments

### 2.1 A block argument is a run of statements passed to a call

A **block argument** is a braced run of statements written at a call site and executed by the callee. Its type is the compiler concept type `@concepts$Block`.

```zane
ran Bool = if(ready) {
    start();
}
```

A block takes no parameters and has no name. It is not a lambda: a lambda is a self-typed function *value* with a complete written type ([`functions.md`](functions.md) §7.2), while a block is a source construct that never becomes a value.

> **See also:** [`syntax.md`](syntax.md) §4.8 for where a block may be written.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#a-block-is-not-a-lambda) — "A block is not a lambda".


### 2.2 A block captures, and does not escape

A block reads and writes the bindings of the scope it is written in, exactly as any other braced block does. Nothing is passed to it.

A block **MUST NOT** escape the call it is written at. It may not be stored, returned, bound to a symbol, placed in a field or element, or spawned. It may be handed on to another verb, because that call is still inside the original call's dynamic extent.

This non-escape makes capture safe here while it stays forbidden for lambdas ([`functions.md`](functions.md) §7.4, [`concurrency.md`](concurrency.md) §5.2). A lambda may be stored and run later, possibly in parallel, so captured state could be reached from code the compiler cannot see. A block runs during the call that receives it, in the frame that wrote it.

A verb that declares a `@concepts$Block` parameter **MUST NOT** be spawned ([`concurrency.md`](concurrency.md) §3.1), and a call to one **MUST NOT** be marked `comptime` ([`effects.md`](effects.md) §5.6).
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#a-verb-with-a-block-parameter-cannot-be-spawned) — "A verb with a block parameter cannot be spawned".


### 2.3 A block is transparent to `return` and `abort`, and an exit ends its run

A block is the scope of the symbols declared inside it, and they are destroyed when it ends, like any other lexical block ([`lifetimes.md`](lifetimes.md) §2.1). Each time a callee runs the block, its scope opens anew and drains when that run ends, so a block that runs repeatedly is a sequence of scopes, one per run, and a spawned call made in one run completes before the next run begins ([`concurrency.md`](concurrency.md) §4.1).

It is **transparent** to `return` and `abort`. Written inside a block, they act on the invocation containing the *call*, not on the block. A `return` inside a block returns from the enclosing verb and carries its value out:

```zane
Int firstNegative(values IntList) {
    i Int = Int(1);
    i!to(values:size()) {
        if(values[i] < Int(0)) {
            return i;
        }
    }
    return Int(0);
}
```

Transparency is what keeps `return` and `abort` usable at any depth: a construct that opened a boundary for them would trap every one written inside it.

An **exit** is the one transfer that a block ends. When an exiting verb such as `core`'s `guard` (§3.6) exits, it ends the current **run** of the innermost block argument around its call. Control leaves that run as though it had reached the block's closing brace, the run's scope drains, and the verb that ran the block carries on:

```zane
Unit reportNonNegative(values IntList) {
    i Int = Int(1);
    i!to(values:size()) {
        guard(values[i] < Int(0));
        console!print(values[i]);
    }
    return Unit();
}
```

The `guard` ends one pass of the repetition: a negative value is not printed, and the next pass runs. In a block written inside another block, the exit ends the inner run only:

```zane
i!to(count) {
    if(ready) {
        guard(skipped);
        start();      // not run when `skipped` is true
    }
    finish();         // runs either way
}
```

A call to an exiting verb **MUST** be written inside a block argument. A verb's body ends in the `return` that produces the verb's value ([`functions.md`](functions.md) §3.5), and an exit carries no value to put in its place. A block argument yields nothing (§2.4), so ending its run needs no value, and a block argument is the only construct an exit can end, whatever the verb around it returns. A `match` arm's body and an abort handler's block are not block arguments: an exit written in one ends the run of the block argument around the `match` or the handled call, and with no such block argument the call is an error.

```zane
Unit configure(settings Settings) {
    guard(settings:isEmpty());    // ILLEGAL: the call is written in no block
    apply(settings);
    return Unit();
}
```

Transparency is a property of the lowering, not a means of leaving a frame. A verb that declares a `@concepts$Block` parameter is **expanded at its call site**, and so is every call it passes the block on to. A block therefore never crosses a call boundary at run time, and a `return` or an exit written inside one is a jump within a single frame. This follows from the block's existing properties — no written type (§2.1), never a value, unable to escape or be spawned (§2.2) — so no frame other than the writing one can ever hold it.

An exit therefore names no scope and unwinds to none, which Zane does not do ([`error-handling.md`](error-handling.md) §4). The exiting verb is an ordinary verb with a frame of its own, and the exit ends that one invocation. The block argument it then ends is written around the call, in the caller's frame, so control resumes at the end of that run, one call out and no further (§4.2).
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#guard-stops-being-a-keyword) — "`guard` stops being a keyword".
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-ends-the-run-of-a-block) — "An exit ends the run of a block".


### 2.4 A block yields nothing

A block has no value. A callee runs it for its effects, through the control-flow intrinsics (§4.1), and nothing written in the block is handed back to the call that receives it.

`resolve` is transparent to a block in the same way as `return` and `abort` (§2.3). Written in a block that sits inside an abort handler, it finishes that handler ([`error-handling.md`](error-handling.md) §4). Written in a block with no handler around it, it is a compile-time error.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#a-block-that-yields-nothing) — "A block that yields nothing".

---

## 3. Branching and Repetition

The declarations in this section are `core` declarations, not language constructs. They are named here because every Zane program uses them, and because their shapes are what the intrinsics of §4 are built to support. Any package may declare others (§4.4).

### 3.1 `if` starts a chain and reports whether it ran

`if(condition) { ... }` runs its block when the condition is `true`, and returns a `Bool` recording whether it did.

```zane
if(ready) {
    start();
}
```

The result need not be bound. An unbound value-type result is discarded ([`lifetimes.md`](lifetimes.md) §4), so a conditional that has no continuation is written as a statement.

### 3.2 `elif` and `else` continue the chain

A chain binds the `if` result to a symbol and continues it with calls on that symbol. `elif` runs its block when no earlier branch has run and its own condition holds, and it writes the chain value; it is therefore a `mut` method called with `!`. `else` runs its block when no earlier branch has run, and only reads the chain value, so it is called with `:`.

```zane
ran Bool = if(age > Int(18)) {
    console!print("adult");
}
ran!elif(age > Int(13)) {
    console!print("teenager");
}
ran:else() {
    console!print("junior");
}
```

Because a chain is a sequence of ordinary calls rather than one construct, its parts are joined by the value they pass along and nothing else. The compiler checks each call; it does not check that a chain is well-formed.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#moving-if-and-loop-into-core) — "Moving `if` and `loop` into `core`".


### 3.3 A condition is evaluated before the call

The condition of an `elif` is an ordinary argument, so it is evaluated before the call even when an earlier branch already ran, like any other argument (§2.4 of [`operators.md`](operators.md) states the same for the `Bool` operators). A condition that must run only when it is reached is written inside the `else` of the chain so far, where it starts a chain of its own:

```zane
ran!elif(cheapFlag) { ... }                         // evaluated whether or not an earlier branch ran
ran:else() {
    late Bool = if(expensiveCheck()) { ... }        // evaluated only if no earlier branch ran
    late:else() { ... }
}
```

The nesting shows at the call site which conditions run unconditionally and which only when reached.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#a-block-that-yields-nothing) — "A block that yields nothing".

### 3.4 Counted repetition advances ordinary storage

Counted repetition is a `mut` method on `Int`. The induction variable is an ordinary local that the block captures; the method advances it through the inclusive range.

```zane
i Int = Int(1);
i!to(Int(3)) {
    console!print(i);
}
```

The block above sees `i = 1`, then `2`, then `3`. Nothing is passed to the block, and no binding is introduced by the call; `i` is the caller's storage throughout.

### 3.5 A loop carries a bound

Counted repetition is the one loop: `repeat` runs a block a written number of times (§4.3). A loop whose stopping condition is a test still has a bound, and returns from the verb when the test passes:

```zane
Unit connect(connection Connection, maxTries Int) {
    attempt Int = Int(1);
    attempt!to(maxTries) {
        if(connection:up()) {
            return Unit();
        }
        log(connection:ping());
    }
    return Unit();
}
```

The `return` leaves `connect`, not only the repetition (§2.3). A `guard` in the repetition's block would end one pass and let the next one run. Zane has no construct that ends a repetition and resumes after it: a search that must carry a result onward `return`s it from inside the block, and work that must follow a repetition sits after a call to the verb that performs it.

A repetition with no meaningful bound — one that runs until its own state reports it is finished — is a worker (§3.7), not a loop.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#doing-without-while) — "Doing without `while`".

### 3.6 `guard` ends the run of its block

`guard(condition)` ends the run of the block argument around its call when the condition is `true`, and does nothing when it is `false` (§2.3). It is a `core` declaration like the rest of this section, not grammar, and its body is the exit intrinsic inside a branch (§4.2).

```zane
Unit configureAll(all SettingsList) {
    i Int = Int(1);
    i!to(all:size()) {
        guard(all[i]:isEmpty());
        apply(all[i]);
    }
    return Unit();
}
```

Its condition is an ordinary argument, so the condition has `core`'s `Bool` as its type and reaches that type through the same coercion as any other argument. A type usable in an `if` is usable in a `guard` on identical terms; the language itself names no type for either.

`guard` takes no block. The statements after it in its block are the statements it skips, and work that should run only when the condition holds is an `if` block of its own.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#guard-stops-being-a-keyword) — "`guard` stops being a keyword".
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-ends-the-run-of-a-block) — "An exit ends the run of a block".

### 3.7 `work` and `register` run a worker

A **worker** is a subject and a **step**: a function value that takes the subject as `this`, advances it, and returns `true` to run again or `false` to stop. `core` declares three ways to run one, as `mut` methods on a subject of any type ([`generics.md`](generics.md) §3.7):

```zane
Unit work(this S Type, step Bool[this S] mut) mut;
Unit register(this S Type, source EventSource, step Bool[this S] mut) mut;
Unit setInterval(this S Type, every Duration, step Bool[this S] mut) mut;
```

`work` calls the step again as soon as it returns `true`, until it returns `false`. `register` calls the step again only once `source` is ready, and waits for `source` in between. An **event source** is a timer or another source the runtime can watch ([`concurrency.md`](concurrency.md) §6.3), and `EventSource.every(interval)` makes a timer that is ready each time `interval` has passed, at most once per call of the step, so a step that ran past several intervals is called once more rather than once for each. `setInterval` is `register` on `EventSource.every(every)`. Its timer counts each `every` from the moment the timer was last ready, not from the end of a step: with `every` at 10 ms, `setInterval` calls the step again 7 ms after a 3 ms step returns, and at once after a 12 ms step returns. None of the three waits before the first step. `Duration` is `core`'s span of time, a whole number of nanoseconds ([`types.md`](types.md) §2.6).

```zane
machine!work(Bool(this Machine) mut {
    this!execute();
    return this.pc <= this.count;
});

clock!register(EventSource.every(Duration.milliseconds(Int(16))), Bool(this Clock) mut {
    this!tick();
    return this.running;
});

clock!setInterval(Duration.milliseconds(Int(16)), Bool(this Clock) mut {
    this!tick();
    return this.running;
});
```

The `register` and `setInterval` calls above run the same worker. While a `register` call waits for its source, every other spawned call and pool worker keeps running ([`concurrency.md`](concurrency.md) §3.4).

The step runs at least once. Its result is the one stopping condition, stated where the step ends. The step is a lambda, so it captures nothing ([`functions.md`](functions.md) §7.4): everything it reads or writes is the subject or reached from it, and the state a worker turns on is gathered in the subject rather than spread through the frame that starts it. A step that cannot stop leaves the worker running for as long as the program does, which is the meaning `work` names.

A step leaves early with `return` and a `Bool`, which continues or stops the worker. A `guard` written directly in the step's body is rejected, since the body is no block (§2.3); one written in a block inside the step ends that block's run.

None of the three methods takes a block, so a caller may spawn any of them, which is how a worker runs without blocking the caller ([`concurrency.md`](concurrency.md) §3.1):

```zane
spawn machine!work(Bool(this Machine) mut { ... });
```

A spawned worker mutates its subject, so the subject is value-typed ([`concurrency.md`](concurrency.md) §4.2), and the scope that spawns it waits for it to finish before it ends ([`concurrency.md`](concurrency.md) §4.1). Many workers that run the same step run in a worker pool ([`concurrency.md`](concurrency.md) §6).

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#workers-replace-the-unbounded-loop-work-and-setinterval) — "Workers replace the unbounded loop: `work` and `setInterval`".

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#register-replaces-workevery-and-the-worker-intrinsics-become-methods) — "`register` replaces `workEvery`, and the worker intrinsics become methods".

---

## 4. Control-Flow Intrinsics

### 4.1 Five intrinsics, stated over primitives, function values, and type parameters

The language provides exactly five control-flow operations:

```zane
@controlflow$branch(condition @primitives$Bool, body @concepts$Block)
@controlflow$repeat(count @primitives$I64, body @concepts$Block)
@controlflow$exitFromCall()
@controlflow$work(this S Type, step R Type[this S] mut) mut
@controlflow$register(this S Type, source @primitives$EventSource, step R Type[this S] mut) mut
```

`branch` executes `body` when `condition` is true and does nothing otherwise; there is no fallback parameter, because the fallback case is `branch` on the complement. `repeat` executes `body` exactly `count` times; a `count` below `1` executes it zero times. `exitFromCall` ends the run of the block that a call to the verb containing it is written in (§4.2). `work` calls `step` on its subject, and calls it again each time it returns `true`, until it returns `false`. `register` does the same, but calls `step` again only once `source` is ready, and waits for it in between ([`concurrency.md`](concurrency.md) §3.4, §6.3). The worker pair introduce `S` at the subject and `R` at the step's result, as a verb introduces a parameter at its first marked occurrence ([`generics.md`](generics.md) §3.2), and the compiler reads both off the arguments. `R` is `@primitives$Bool` or a type with an implicit constructor to it, and each result reaches `@primitives$Bool` through that constructor, as a condition reaches `branch`; the compiler therefore accepts a step that returns `core`'s `Bool` as written.

The worker pair are `mut` methods whose subject is a type parameter ([`generics.md`](generics.md) §3.7). A program calls them with their intrinsic namespace as the qualifier of a qualified method call ([`functions.md`](functions.md) §6.2): `this!@controlflow$work(step)`. The subject is the only value a control-flow intrinsic writes. The call borrows the subject mutably, as every `!` call borrows its subject, whether the subject is a writable place or a temporary, and every `step` runs on that one borrow ([`functions.md`](functions.md) §2.4). The call settles the subject once, before the first step, so each step writes the place the call named. A caller therefore calls the pair on a place it may write, most often its own `this` under `mut`.

None takes a fundamental type. The block-taking three take **storage primitives** or nothing at all, and the worker pair take a subject of any type, a function value over it, and, for `register`, a primitive event source. That is what separates control flow from the language: an intrinsic depends on no declaration in any package, so `core` is an ordinary consumer of them rather than a privileged part of the compiler.

Every argument of a control-flow intrinsic is a coercion site ([`types.md`](types.md) §4.2), and the compiler coerces no method's subject, a worker's included. A caller therefore writes ordinary values and never unwraps anything: a `Bool` reaches `branch` through the implicit constructor `core` declares to `@primitives$Bool`. The compiler rejects a bare literal, because a primitive has no implicit constructor from a concept type ([`types.md`](types.md) §2.7). The compiler accepts a type that declares its own conversion on the same terms.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#two-intrinsics-and-what-they-are-stated-over) — "Two intrinsics, and what they are stated over".


### 4.2 `exitFromCall` ends the run of its caller's block

`@controlflow$exitFromCall()` makes the verb whose frame holds it an **exiting verb**. That frame is the verb's body and the blocks written there, which are expanded into it (§2.3). When the intrinsic runs, it ends the exiting verb's invocation, and with it the current run of the block that the call to the exiting verb is written in. Control resumes after that run, in the verb that ran the block.

It reaches that block and stops. In `outer` → `helper` → `guard`, the call to `guard` is written in a block inside `helper`: the exit ends that block's run, `helper` goes on after it, and `outer` is unaffected.

Ending only the caller's block makes an exit declarable. A verb whose body holds the intrinsic ends the block of whoever calls it, so `core` declares the exit every program uses (§3.6) rather than the language spelling one as grammar:

```zane
package core;

Unit guard(condition Bool) {
    if(condition) {
        @controlflow$exitFromCall();
    }
    return Unit();
}
```

Which block that is follows from where the call is written, not from where the intrinsic sits. A verb declaring a `@concepts$Block` parameter is expanded at its call site and has no frame of its own, so the intrinsic inside `guard`'s `if` block is in `guard`'s frame, and the call to `guard` written inside a block belongs to that block:

```zane
Unit main() {
    if(true) {
        guard(true);
        console!print("not reached");
    }
    console!print("reached");
    return Unit();
}
```

The exit ends the run of the `if` block, and `main` goes on to print `reached`.

A call to an exiting verb is legal only inside a block argument (§2.3). A verb's body ends in the `return` that produces its value, and the exit carries none, so it cannot end a body; a block yields nothing, so the exit can end its run in any verb, whatever that verb returns. Writing the intrinsic in a verb makes the verb an exiting one: when the intrinsic runs, a call to the verb ends its caller's block as well, so the intrinsic never stops the verb alone. To stop only itself, a verb uses `return`.

The intrinsic **MUST NOT** appear in a lambda's frame, including a block argument written there. A call through a function value names the value, not a body, and a function type does not record whether the function exits, so a call through one could not be checked against the rule above. A lambda leaves early with `return`. A call to an exiting verb such as `guard`, written in a block inside the lambda, ends that block's run.
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#guard-stops-being-a-keyword) — "`guard` stops being a keyword".
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#no-exitfromcall-in-a-lambda-body) — "No `exitFromCall` in a lambda body".
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-ends-the-run-of-a-block) — "An exit ends the run of a block".

### 4.3 A loop is bounded by the shape of `repeat`, and a worker names its recurrence

`repeat` takes a count, so a single invocation runs its body at most that many times and every loop built on it carries a written bound, whoever declares it. The bound counts passes; a pass that calls an unbounded form (below) runs as long as that form does. The guarantee is a property of the intrinsic rather than of who may call it.

A repetition without a written bound has two forms, and a reader can recognize each in the source. Recursion re-enters a verb, which can re-enter `repeat` without limit. A worker (`work`, `register`, or one of a worker pool's, [`concurrency.md`](concurrency.md) §6) runs its step until the step returns `false`. A worker's step is a function value that captures nothing (§3.7), and the place the repetition starts is the call that names it. A `work` or `register` step depends on no state but the call's subject. A worker pool's step depends on its element and on the pool's shared state. The step receives that state read-only, and a change to the state can schedule the step's next call ([`concurrency.md`](concurrency.md) §6.3).
> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#two-intrinsics-and-what-they-are-stated-over) — "Two intrinsics, and what they are stated over".


### 4.4 Any package may declare control flow

A package that wants a repetition policy, a branching form, or a scoped resource construct declares a verb taking a `@concepts$Block` parameter and calls the control-flow intrinsics, exactly as `core` does for §3.

```zane
Unit twice(body @concepts$Block) {
    @controlflow$repeat(Int(2), body);
    return Unit();
}

twice() {
    console!print("again");
}
```

The intrinsic namespaces are reachable from every package without an import ([`syntax.md`](syntax.md) §2.7), and the coercion of §4.1 supplies the primitive, so no package is closer to the intrinsics than any other. `core`'s declarations in §3 have no standing the example above lacks. Being the fundamental types' home package gives `core` first place in unqualified method lookup, not exclusive rights: any package may declare methods on them, reached by a qualifier where lookup does not find them ([`functions.md`](functions.md) §6.1 and §6.3).

Exits are declarable on the same footing, and need no block parameter. An `@controlflow$exitFromCall()` in any verb's body makes a call to that verb end the run of the block it is written in whenever the intrinsic runs (§4.2), so a package may declare an exiting verb of its own exactly as `core` declares `guard`. An exit spelled as grammar could never be declared that way: a keyword can only exit where it is written, so it cannot be handed to anyone.

---

## 5. 1-Based Ordinal Counting

### 5.1 Positional indexing is 1-based

When an `Int` identifies an ordinal position in an ordered sequence, the first position is `1`. For a sequence with `n` elements, the positional index range is therefore `1` through `n`.

```zane
lastPosition Int = list:size();
element Int = list[lastPosition];
```

The example above selects the last element by using the sequence size directly as the final valid position.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#counting-from-one) — "Counting from one".

### 5.2 A position outside its sequence stops the program

A sequence of `n` elements has the positions `1` through `n`. An intrinsic that takes a position checks it, and a position outside that range **stops** the program ([`packages.md`](packages.md) §6.3). A stop is not an abort, so no handler sees it. The subscripts of the container primitives and the string primitive's `byteAt` check their position this way ([`generics.md`](generics.md) §8.6, [`types.md`](types.md) §2.12).

An intrinsic that takes a **range**, a `count` of positions from a position `start`, checks it the same way. The range lies in a sequence of `n` elements when `count` is at least `0`, `start` is at least `1`, and `start + count - 1` is at most `n`. A range of no positions may therefore start at `n + 1`, just past the last element. Any other range stops the program.

```zane
list List<Int> = List(Int);
list!push(10);
list!push(20);
last Int = list[2];               // 20
list!appendOwn(3, 0);             // legal: no elements, starting just past the end
missing Int = list[3];            // stops the program: list has no position 3
```

The section that specifies each intrinsic names the position or range it checks, along with any exception it makes: `findAny` and `findPair` give `0` for a `count` below `1` instead of checking their range ([`types.md`](types.md) §2.12).

> **Story:** [`stories/generics.md`](../stories/generics.md#the-list-primitives-operations-are-specified-and-a-position-outside-a-list-stops-the-program) — "The list primitives' operations are specified, and a position outside a list stops the program".

---

## 6. Summary

| Concept | Rule |
|---|---|
| Block argument | A braced run of statements passed to a call; type `@concepts$Block`; no parameters, no name, never a value |
| Capture | A block reads and writes its enclosing scope's bindings |
| Escape | A block may not be stored, returned, bound, placed in storage, or spawned; it may be handed to another verb |
| Scope | A block is the scope of its own declarations and is transparent to `return` and `abort`; an exiting verb such as `guard`, when it exits, ends the run of the innermost block argument around its call, and a call to one is legal only in a block argument |
| Lowering | A verb taking a block parameter is expanded at its call site, transitively, so a block never crosses a call boundary and a `return` or an exit inside one is a jump within one frame |
| Yielding | A block yields nothing; `resolve` in one finishes the handler around the call, like `return` and `abort` |
| Branching | `if` returns whether it ran; `ran!elif(...)` continues the chain and writes it; `ran:else()` ends it — all `core` declarations |
| Condition evaluation | A condition is an ordinary argument and is evaluated before the call; one that must run only when reached is nested in the chain's `else` |
| Counted repetition | `i!to(end)` advances the caller's own `Int` and captures it in the block |
| Worker | `subject!work(step)` runs a non-capturing `Bool[this S] mut` step on the subject until it returns `false`; `subject!register(source, step)` runs it again each time the event source is ready, and `subject!setInterval(every, step)` is `register` on a timer; none takes a block, so each may be spawned |
| Control-flow intrinsics | `@controlflow$branch`, `@controlflow$repeat`, `@controlflow$exitFromCall`, `@controlflow$work`, and `@controlflow$register`; stated over `@primitives$Bool`, `@primitives$I64`, `@primitives$EventSource`, a step function value whose result converts to `@primitives$Bool`, a subject of any type, or nothing; the worker pair are `mut` methods of their subject, called as `subject!@controlflow$work(...)`; reachable from any package, with ordinary values reaching them through the implicit constructors their types' packages declare |
| Bounded repetition | `repeat` takes a count, so one invocation runs its body at most that many times and every loop built on it carries a written bound; recursion and workers are the unbounded forms, and each is recognizable in the source |
| Exit | `@controlflow$exitFromCall()` makes the verb holding it, when the intrinsic runs, end the run of the block its call is written in, which is what lets `core` declare `guard` as an ordinary verb; the exit carries no value and a block yields none, so a call to an exiting verb is legal only in a block; a lambda's frame may not contain the intrinsic |
| Ordinals | Positions and counted repetition start at `1`; the last valid position is the size |
| Out of range | A position outside `1` through `n`, or a range of `count` positions from `start` that does not lie in the sequence, stops the program; a range of no positions may start at `n + 1` |
