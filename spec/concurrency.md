# Zane Concurrency Model

This document specifies Zane's concurrency model: compiler-managed parallelism, the `spawn` keyword, worker pools, and the safety rules that govern concurrent execution.

> **See also:** [`effects.md`](effects.md) §3 for what each kind of verb may write. [`lifetimes.md`](lifetimes.md) §2 for lifetime rules. [`syntax.md`](syntax.md) §4 for `spawn` syntax.

---

## 1. Overview

Zane separates **parallelism** (compiler-managed, unobservable) from **concurrency** (explicit, programmer-controlled).

- **`Implicit parallelism`.** The compiler may run provably independent work in parallel when it cannot change program results.
- **`Explicit concurrency`.** `spawn` starts a concurrent function or method call; ordering is the programmer’s responsibility. A worker pool runs bulk work: many workers that run the same step, each on an element of its own.
- **`Water-tower lifetimes`.** Objects owned in a scope live until all spawned work in that scope completes.
- **`Mutation needs a value subject`.** A spawned call may mutate only a value-typed subject; a value type's transitive alias-freedom lets the compiler rule out a data race from the subject's type, and at most one spawn may mutably borrow a given location. A worker pool's element may also be a reference type whose every `&` names a constant.
- **`Plain calls return in order`.** A call without `spawn` returns before the statement after it runs; the caller chooses concurrency at the call site, and a verb's signature is the same either way.

---

## 2. Implicit Parallelism (Compiler Responsibility)

### 2.1 Parallelization of independent work

The compiler analyzes a program's work for independence. It may schedule independent work in parallel when:

- the effect system proves the work is non-conflicting
- parallelization is profitable (based on instruction count, IO presence, or other heuristics)

A call that writes nothing and touches no capability-backed state may run in parallel whether or not it terminates.

This parallelism is **unobservable**: it must not change output, only timing.

### 2.2 Thread configuration

The runtime uses a work-stealing thread pool. It starts sized to hardware concurrency, and the program's runtime ([`effects.md`](effects.md) §6.6) resizes it:

```zane
@primitives$Unit?@primitives$Unit setThreads(this @runtime$Runtime, count @primitives$I64) mut
@primitives$Unit setThreadsAuto(this @runtime$Runtime) mut
```

`setThreads` sizes the pool to `count` threads. A `count` below `1` aborts the call and leaves the pool as it was, so every call carries a handler ([`error-handling.md`](error-handling.md) §3.1). `setThreadsAuto` sizes the pool to hardware concurrency again, and cannot abort.

```zane
@program$runtime!setThreads(Int(8)) ? ignored {
    resolve @primitives$Unit();
}
@program$runtime!setThreadsAuto();
```

Either may be called at any time and any number of times. Compiler-scheduled parallelism changes only timing (§2.1), and spawned work may depend on scheduling at any pool size (§3.7), so resizing the pool changes how fast a program runs without making any result possible that was not possible before. Each call writes to the runtime, so outside the root package only a `mut` method whose `this` reaches the runtime can make one ([`effects.md`](effects.md) §3).

> **Story:** [`stories/effects.md`](../stories/effects.md#where-the-first-capability-comes-from) — "Where the first capability comes from".

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#parallelism-you-cant-see-concurrency-you-must-ask-for) — "Parallelism you can't see, concurrency you must ask for".

---

## 3. `spawn` and Explicit Concurrency

### 3.1 `spawn` targets function and method calls only

`spawn` starts a concurrent **function or method call**. Both forms expose a verb signature for effect and conflict analysis.

A program **MUST NOT** spawn a verb that declares a `@concepts$Block` parameter. A block captures the frame that wrote it ([`control-flow.md`](control-flow.md) §2.2), and capture is safe there only because the block runs inside that call. Spawning one would put captured state in a parallel task, which is exactly what §5.2 forbids for values. Since branching and repetition are such verbs ([`control-flow.md`](control-flow.md) §3), this is also what makes `spawn` on a conditional or a loop illegal. A worker takes a function value rather than a block, so a repetition that should run beside its caller is a spawned worker ([`control-flow.md`](control-flow.md) §3.7).

```zane
spawn runServer(8080);            // ok: function call
spawn server:listen(8080);        // ok: read-only method call
spawn server!refreshConnections(); // ok: mutating method call
spawn machine!work(step);         // ok: a worker takes a function value
spawn if(cond) { f() }            // ILLEGAL: `if` takes a block argument
```

### 3.2 Spawned values block on read

A spawned call that returns a value can bind to a symbol. Reading that symbol blocks until the spawned call returns.

```zane
result String = spawn listen(8080);
console!print(result); // blocks until listen returns
```

### 3.3 Abortable spawned calls are handled at the spawn site

If a spawned call is abortable, it must still attach `?` or `??` directly to the `spawn` expression. There is no separate propagation form at result-read time.

```zane
port Int = spawn listen(8080) ? err {
    resolve Int(404);
}

fallback Int = spawn listen(8081) ?? Int(404);
```

The bound symbol has the handled primary type, and reading it still blocks until the spawned call finishes.

The call **settles** once, on the thread that made the spawn: at the first read of the bound symbol, or, when nothing has read it by then, where the block the spawn is written in ends. Settling waits for the call to finish. When the call aborts, its handler runs there, and when the spawned verb is an exiting one and exits, the run of the block the spawn is written in ends there ([`control-flow.md`](control-flow.md) §2.3). A later read sees the settled value and runs no handler.

```zane
// Int?String safeDivide(a Int, b Int) aborts when b is zero
runs Int(0);
q Int = spawn safeDivide(Int(9), Int(0)) ? e {
    runs = runs + Int(1);   // runs here, at the first read below
    resolve Int(7);
}
check(q == Int(7));
check(q == Int(7));         // settled already: the handler does not run again
```

A block left before it ends, by a `return`, an exit, or an `abort`, still waits for the call as it drains (§4.1), but runs no handler, and the call's outcome is discarded with the block.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#a-spawns-handler-and-its-loans-end-where-its-block-does) — "A spawn's handler and its loans end where its block does".

### 3.4 A wait holds back only its caller

The language has two calls that wait. A read of the symbol that holds a spawned call's result waits until that call returns (§3.2). A call to `register` waits for its event source between one call of its step and the next ([`control-flow.md`](control-flow.md) §3.7).

A waiting call holds back only its caller: the statements after the call run once the wait is over. Every other spawned call, and every worker of every worker pool (§6), keeps running while the wait lasts. The rule applies wherever the waiting call occurs: in a spawned call, in a worker pool's step, or in code that runs without `spawn`. For example, a pool step that reads a spawned call's result holds back only its own worker, and the pool's other workers keep calling their steps.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#parking-is-dropped-a-wait-holds-back-only-its-caller) — "Parking is dropped: a wait holds back only its caller".

### 3.5 A call that waits needs no `spawn`

A call that waits is an ordinary call. Without `spawn`, the call returns once its wait is over, and the statement after it runs only then (§5.3). With `spawn`, the call runs beside its caller, which continues at once.

### 3.6 Ordering is explicit

The compiler does not reorder `spawn` calls. The order in the source is the order in which spawns are started, and any blocking read happens exactly where written.

Independent work may still be parallelized only when doing so preserves those source-visible points.

### 3.7 No serial-equivalence guarantee

`spawn` explicitly opts out of serial equivalence. Program results may depend on scheduling except where constrained by effect and ownership rules.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#spawn-and-why-it-marks-only-a-call) — "`spawn`, and why it marks only a call".

---

## 4. Safety Rules Under Concurrency

### 4.1 Water-tower lifetime extension

A scope does not complete until all `spawn`ed calls inside it have completed, and every worker of a pool it owns has finished (§6.7). Objects owned in that scope are destroyed only when the scope is **drained**, except a pool's elements, each destroyed when its own step returns `false` (§6.2).

The analogy is a vertical water tower with water at the top and one horizontal plate for each still-running spawned call in that scope. The water cannot fall past a plate that is still in place, so destruction cannot pass that still-live concurrent work either.

Each time one spawned call finishes, one plate is removed. The water level drops to the next remaining plate. Only when the last plate is gone does the water reach the bottom of the tower. That moment is when the scope drains and ordinary destruction runs.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#the-water-tower-lifetimes-that-survive-the-spawn) — "The water tower: lifetimes that survive the spawn".

### 4.2 Concurrent mutation requires a value-typed subject

A spawned call may **mutate** state only through a value-typed subject. A `mut` call whose subject is a reference type is a compile-time error at the spawn site. Outside the root package's and exported lambdas' writes to the program's console and runtime ([`effects.md`](effects.md) §6.6), the subject is the only path by which any call writes state its caller can see, because every other parameter, and every reference derived from one, is read-only ([`effects.md`](effects.md) §4.1, §4.4). The rule is sound because a value type is transitively alias-free — it contains no reference-type or `&` field anywhere downstream (see [`memory.md`](memory.md) §2.10) — so no two names can reach the same mutated object by different paths. A value that owns **boxed members** is no exception: a box holds an instance of the member's own type, and a value copy is deep (see [`memory.md`](memory.md) §2.3), so two values never reach one payload. The compiler therefore rules out an aliased data race from the subject's *type* alone, with no whole-program alias analysis.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#closing-gaps-in-the-two-signature-safety-rules) — "Closing gaps in the two signature-safety rules".

> **Story:** [`stories/effects.md`](../stories/effects.md#a-mutating-call-is-a-write) — "A mutating call is a write".

A direct consequence is that spawned work never mutates a reference-typed object, so every concurrent **read** of the reference-typed object graph is safe by construction. The program's console and runtime are one exception: a spawned verb in the root package may write them, and those writes are serialized like any access to capability-backed state (§4.5). A worker pool's element is the other: it may be a reference type, because no `&` reachable from it names an object the program writes (§6.4).

### 4.3 Single writer per storage location

For any one storage location, at most one live spawned call may hold a **mutable borrow** — the `!` subject of a spawned `mut` call. By §4.2 that subject is always value-typed, so every borrow this rule counts is a value borrow ([`memory.md`](memory.md) §2.9). Two spawned calls that mutably borrow the same location are a compile-time error. Because value types carry no `&`, a location's identity is unambiguous — there is no hidden alias to obscure that two subjects denote the same slot — so this disjointness is checked at the spawn site by inspecting the subjects, not by tracing the program. The spawning scope may not access a location while a live spawn holds its mutable borrow; the borrow is released when that spawn completes (§4.1).

A spawn is also **lent** every owner passed to it, directly or through a reference, other than one it takes as a `^T` argument, which moves into the call ([`memory.md`](memory.md) §2.9). The spawned call may read a lent owner until the scope the spawn is written in drains (§4.1). The spawning scope keeps running meanwhile, so while the loan lasts that scope and the blocks inside it **MUST NOT** write a lent owner: not by assignment, not as the subject of a `!` call, and not by moving it out. A write reached through a reference is a write of the owner the reference names. A spawned `mut` call on a value-typed part of a lent owner is not such a write: it is a spawned mutation, which the reading spawn observes as a coherent snapshot (§4.4).

```zane
// Int glance(d &Dial) => d.reading.level;
view &Dial = dial;
seen Int = spawn glance(view);   // lent `dial`, through `view`
dial.reading = Gauge(Int(3));    // ILLEGAL: `glance` may be reading `dial`
spawn dial.reading!nudge();      // legal: a spawned mutation of part of `dial`
```

With this rule, every concurrent read of a lent owner is safe whoever writes: no spawned call writes a reference-typed object (§4.2), the spawning scope writes no lent owner, and a spawned mutation is read as a snapshot.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#a-spawns-handler-and-its-loans-end-where-its-block-does) — "A spawn's handler and its loans end where its block does".

A spawn site inside a loop body holds one live borrow at a time. Each pass of the body is a scope, and it drains only once that pass's spawned calls have completed (§4.1), so the passes run one after another and a spawn of one pass never overlaps a spawn of the next. Many workers that should run the same step at once run in a worker pool instead (§6).

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#closing-gaps-in-the-two-signature-safety-rules) — "Closing gaps in the two signature-safety rules".

### 4.4 Reads take a coherent snapshot

A spawned call may read a value that another live spawn is mutating; the read observes a **coherent snapshot** of the value rather than blocking. Reading a shared value into a fresh binding — `snap VarType = shared` — is what takes the snapshot, and the copy is tear-free even when the writer is mid-update. This replaces lock-based serialization for in-memory value state, so a real-time reader never waits on a writer. Serialization still applies to external, capability-backed resources (§4.5).

A value whose members are all inline is one fixed-size contiguous slot, so the snapshot is a fixed-size read validated and retried against the writer's updates. A value that owns **boxed members** (see [`memory.md`](memory.md) §3.3) is instead *walked*: the reader follows each handle and copies the payload it names, and the writer may free and recycle that block in between. Four rules make that walk safe and bounded.

- **A stale read is garbage, not an invalid access.** A scope cannot drain while a spawn inside it is live (§4.1), and the runtime unmaps a scope's chunks only at drain (see [`memory.md`](memory.md) §3.2). A block freed during the walk is therefore recycled *within a mapping that stays live*, so a stale handle resolves into readable memory holding some other occupant's bytes. This is the same failure class as a torn flat read, and the version check discards it the same way.
- **Structure-directing metadata is untrusted until validation succeeds.** Before the final version check, every byte the walk reads may be torn or may belong to a recycled occupant. The walk **MUST** validate any metadata before using it to select a typed layout or traversal shape. In particular, a `variant` or `#variant` discriminant must name one of the type's declared cases before case dispatch; an invalid discriminant aborts the attempt and causes a retry. The same rule applies to any count, length, or other metadata used to decide which child handles exist or how many entries to visit. A valid but stale value may still describe the wrong case for the attempted snapshot, but the remaining checks keep that attempt safe and the final version check discards it.
- **The walk MUST be bounded, and MUST validate a whole payload span.** Because a recycled block may hold a handle left by its next occupant, a reader may pick up an address that is not part of the structure it is traversing. Before interpreting what a handle names, the walk **MUST** check that the handle's **complete payload span** — its base address, plus the size of the member's declared type, at that type's alignment — lies within a live region of the scope, and it **MUST** stop at a **depth bound**. That bound is derived, not arbitrary: a correct walk descends through a distinct live block at every step, so the number of live blocks in the scope's dynamic region when the attempt begins is a depth no legal structure can reach. Fixing the bound there keeps exhaustion **retryable** rather than terminal — exceeding it always shows that the walk is following recycled bytes, and never that the value is legitimately too deep, so the attempt is discarded and retried like any other failed validation. Each attempt takes the bound afresh, since the region may have grown. An address that merely lands in a live region is not enough: a recycled block can hold one near a region's end, and reading a payload's worth of bytes from there would run past it. A handle failing either check aborts the attempt rather than being followed.
- **The reader allocates in the destination scope.** A deep snapshot is an ordinary deep value copy into its fresh destination binding (see [`memory.md`](memory.md) §2.3). Each boxed payload is therefore allocated from the size stacks of that binding's scope, not generally from the writer's or source value's scope. Snapshotting introduces no special source-scope staging and does not by itself make the reader and writer contend on one stack. Allocator synchronization is required only when concurrent work actually shares an underlying arena. Every block allocated by a snapshot attempt remains **provisional** until the final version check accepts that attempt. If metadata validation, span or depth validation, or the final version check rejects the attempt, the runtime **MUST** return every block allocated by that attempt to the destination scope's corresponding size stacks before retrying. The destination binding becomes live only after the attempt is accepted.

Two costs follow and are accepted. A snapshot of such a value allocates and is O(structure) where a flat snapshot allocates nothing, and a retry redoes the whole walk, so a fast writer can starve a reader, which it cannot do for a flat value. A value type that is read under `spawn` on a hot path is therefore better kept flat.

> **Story:** [`stories/memory.md`](../stories/memory.md#what-a-copy-is-for-and-the-ban-that-survived-it) — "What a copy is for, and the ban that survived it".

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#value-typed-mutation-closing-the-aliased-write-gap) — "Value-typed mutation: closing the aliased-write gap".

### 4.5 Effect conflicts on external resources

The effect system classifies resource access as **read** or **write**. Two accesses to external, capability-backed state conflict only when they reach the same state: the program's console, its runtime, a foreign capability's own state, or one outside handle's ([`ffi.md`](ffi.md) §5.6). Concurrent accesses to the same state are permitted only when they do not conflict:

- read/read: allowed
- read/write: serialized
- write/write: serialized

The compiler enforces this from effect signatures; the programmer does not add locks.

### 4.6 References passed to spawned work remain independent

When a reference is passed to a spawned call, the callee receives its own reference to the same owner. Rebinding the caller's `&` symbol later changes only the caller's storage; it does not retarget the reference already held by spawned work.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#safety-the-compiler-proves-from-signatures-not-locks) — "Safety the compiler proves from signatures, not locks".

### 4.7 A host may call exports at once

A host may call the exported lambdas of a dynamic library ([`packages.md`](packages.md) §8) from several threads at once; a CLAP host, for one, calls `process` on its audio thread while its main thread makes the GUI calls. Exports carry no thread annotation. Every host obeys one rule, the **host rule**: it never runs two calls that borrow the same C-held object mutably at the same time, and never runs a call that takes a C-held object's owner as `^T` alongside any other call on that object. The compiler then checks three conditions on every exported lambda:

- **It writes only its subject.** An exported lambda writes the C-held object of its `this T mut` subject, as any `mut` method writes its subject ([`effects.md`](effects.md) §3), and capability-backed state, which §4.5 orders. The host rule keeps every other writer of that object away during the call.
- **It reads every C-held object it borrows read-only as a snapshot,** a `this T` subject included. Such an object may have a writer running, so the lambda reads it as a coherent snapshot (§4.4): tear-free and never blocking. A snapshot of a flat value-typed field is one fixed-size read, while a snapshot of a value with lists walks and copies the whole value, so a lambda on a hot path reads flat fields.
- **No second path reaches its subject.** An exported lambda whose subject is `this T mut` takes no other parameter through which a `T` is reachable: no second `T`, no `&T`, and no C-held type with a `T` or `&T` inside, since C might pass the same pointer twice. Two read-only borrows of one object stay allowed, because reads do not conflict.

```zane
type SynthInstance = #struct {
    gui synth$Synth;
    request synth$Params;   // written by GUI calls, read by process
}

type AudioState = #struct {
    dsp synth$Dsp;
    shown synth$Params;     // written by process, read by GUI calls
}

process clap$Output(this AudioState, ui SynthInstance, block clap$Block) mut {
    out clap$Output(block);
    out!add(this.dsp!follow(ui.request));   // a snapshot of one flat field
    out!audio(this.dsp!render(block));
    this.shown = this.dsp:params();
    return out!finish();
} export "synth_process"
```

Outside a worker pool's steps (§6.2), exported lambdas are the only calls that run at once with others and write a reference-typed object, which §4.2 forbids a spawned call. The difference lies in where the object lives. A spawned call's subject lives in a scope that other code shares, and other code may reach a reference-typed subject through `&` values the compiler cannot see. A C-held object lives in a top-level scope that no other object reaches ([`packages.md`](packages.md) §8.5), so the argument C passes is its only path, and the store rule proves that no alias exists. The host, not the compiler, orders the writers. The closest precedent is a worker pool's element (§6.4), which may be a reference type because the pool alone owns it and every `&` it reaches names a package constant or a capability.

Each C-held object has one host call writing it at a time, so two host calls never allocate in its top-level scope at once; spawned calls inside one host call that share the scope synchronize as §4.4 requires of any work that shares an arena. The compiler trusts the host on two points, as it trusts a foreign signature ([`ffi.md`](ffi.md) §6): a pointer C passes names a live object of the right type, and the host obeys the host rule.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#why-exports-may-do-what-spawns-may-not) — "Why exports may do what spawns may not".

---

## 5. Concurrency Boundaries

### 5.1 No cancellation or shutdown ordering

The language does not provide cancellation, kill groups, or shutdown ordering. A spawned call runs until it returns or the process is terminated externally.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#what-the-core-deliberately-leaves-out) — "What the core deliberately leaves out".

### 5.2 Lambdas do not capture

Lambdas **MUST NOT** capture outer variables. All dependencies must be passed explicitly. This keeps effect tracking and the value-subject check (§4.2) tractable: a function value may be stored and run later, so captured state could be reached from a frame the compiler is no longer looking at.

A **block argument** captures and is exempt, because it cannot be stored and cannot be spawned (§3.1). It runs during the call that receives it, in the frame that wrote it, so the reach of what it captures is the reach of ordinary lexical code. See [`control-flow.md`](control-flow.md) §2.2.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#a-verb-with-a-block-parameter-cannot-be-spawned) — "A verb with a block parameter cannot be spawned".

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#safety-the-compiler-proves-from-signatures-not-locks) — "Safety the compiler proves from signatures, not locks".

### 5.3 A call without `spawn` returns before the next statement runs

A call made without `spawn` returns before the statement after it runs. Every such call keeps this order: no verb declaration lets a plain call return early, and implicit parallelism (§2) changes only timing, never the order the program observes. Only `spawn` and a worker pool's `start` (§6) run work alongside the statements after them, and the caller chooses either at the call site. A verb's declaration is therefore the same whether a caller calls it plainly, spawns it, or runs it as a pool's step.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#parallelism-you-cant-see-concurrency-you-must-ask-for) — "Parallelism you can't see, concurrency you must ask for".

### 5.4 No language-level process or channel abstraction

Zane does not define a dedicated `Process` type, actor primitive, or channel primitive in the core language. A program expresses long-running concurrent work as ordinary spawned function or method calls, such as a spawned worker ([`control-flow.md`](control-flow.md) §3.7), and bulk work as a worker pool (§6), plus explicit state flow governed by ownership and effect rules. A worker pool is neither an actor system nor a channel: its workers send events only to its own funnel, the funnel sends nowhere, and nothing sends a message to a particular worker or element.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#what-the-core-deliberately-leaves-out) — "What the core deliberately leaves out".

---

## 6. Worker Pools

### 6.1 A worker pool runs bulk work

`spawn` starts one call that runs alongside the rest of the program. A **worker pool** runs bulk work: many workers that run the same step, such as one worker per network connection. Each worker in a pool has the two parts of any worker ([`control-flow.md`](control-flow.md) §3.7): an **element**, which plays the role of the subject, and a **step**. The pool's workers may share a state, which only the pool's **funnel** writes.

`@primitives$WorkerPool<T, S, E>` is a reference-type primitive. `T` is the element type, `S` the shared state, and `E` the event a step publishes to the funnel. `core` declares `WorkerPool<T, S, E>` over it, and `EventSource` over `@primitives$EventSource` (§6.3). The primitive's two constructors take the element type as an explicit parameter ([`generics.md`](generics.md) §5.3):

```zane
@primitives$WorkerPool<T, S, E>(T Type, state S Type, funnel @primitives$Unit[this S, E Type] mut)
@primitives$WorkerPool<T, @primitives$Unit, @primitives$Unit>(T Type)
```

The first builds a pool with shared state `state` and the funnel that writes it. `S` **MUST** be a value type, since workers read it by snapshot (§4.4). `E` **MUST** be a value type too, since a published event waits in the funnel's queue after the step that published it has returned (§6.5). The second builds a pool whose workers share nothing.

```zane
chat WorkerPool(Session, Room(), Unit(this Room, event Said) mut {
    return this!post(event);
});
```

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#spawn-in-a-loop-runs-one-pass-at-a-time) — "`spawn` in a loop runs one pass at a time".

### 6.2 `start` adds a worker

```zane
@primitives$Unit start(this @primitives$WorkerPool<T, S, E>, element ^T, step @primitives$Bool[this T, S, @primitives$Unit[E]] mut) mut
@primitives$Unit start(this @primitives$WorkerPool<T, S, E>, element ^T, sources @primitives$List<@primitives$EventSource>, step @primitives$Bool[this T, S, @primitives$Unit[E]] mut) mut
```

`start` takes the element into the pool and returns at once. A reference-typed element moves in, and a value-typed one is copied in, as a `ListRef` takes its elements ([`memory.md`](memory.md) §2.9). The step then runs concurrently with the caller and with every other worker. Each call of the step receives the element as `this`, the pool's state as a read-only parameter, and a function value that publishes an event to the funnel. The state a step call receives is a coherent snapshot taken when the call begins (§4.4), so that state does not change during the call. When the step returns `false`, the element is destroyed and leaves the pool.

A pool built without state has `start` overloads whose step takes only its element:

```zane
@primitives$Unit start(this @primitives$WorkerPool<T, @primitives$Unit, @primitives$Unit>, element ^T, step @primitives$Bool[this T] mut) mut
@primitives$Unit start(this @primitives$WorkerPool<T, @primitives$Unit, @primitives$Unit>, element ^T, sources @primitives$List<@primitives$EventSource>, step @primitives$Bool[this T] mut) mut
```

```zane
chat!start(session, [EventSource.every(Duration.milliseconds(Int(50)))], Bool(this Session, shared Room, publish Unit[Said]) mut {
    return this!talk(shared, publish);
});
```

`start` writes only the pool's own record of its elements, never an element a worker holds, so it may be called while the pool's workers run.

### 6.3 When a step runs

The runtime never runs two calls of one element's step at once. Steps of different elements may run in parallel.

A worker started without event sources has its step called again as soon as it returns `true`. A worker started with **event sources** has its step called again when one of them is ready, or when the funnel has changed the shared state since the step's last call. Between calls, such a worker waits without occupying a thread. With or without event sources, the step's first call comes right after `start`.

An event source is an `@primitives$EventSource`. The named constructor `@primitives$EventSource.every` makes a timer from an interval in nanoseconds, an `@primitives$I64`, which is ready each time that interval has passed since it was last ready. An interval below `1` makes it always ready. A timer is ready at most once per call of the step it wakes: when a call begins with the timer ready, the timer's next deadline is the first deadline on its schedule after the call begins, so an interval that passes while the step runs makes it ready again when the step returns, so a step that ran past several intervals is called once more, not once for each interval it missed. A `register` on a timer ([`control-flow.md`](control-flow.md) §3.7) counts its timer the same way. `core`'s `EventSource.every` takes a `Duration` instead. Other event sources come from the intrinsics that provide them. A worker outside a pool waits on an event source through the `register` method ([`control-flow.md`](control-flow.md) §3.7).

A step is designed never to wait: it reads only values that already exist, and then it returns. A step that does wait, in a `register` call or on a spawned call's result, holds back only its own worker (§3.4).

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#the-runtime-wakes-a-worker-when-its-event-sources-are-ready) — "The runtime wakes a worker when its event sources are ready".
> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#a-publish-goes-to-the-running-pool-and-missed-ticks-merge) — "A publish goes to the running pool, and missed ticks merge".

### 6.4 An element may reference constants

An element is a value type, or a reference type every `&` of which, however deep, names a **constant**: a package constant ([`packages.md`](packages.md) §5.1) or a capability. This rule is an exception to §4.2 that keeps the guarantee §4.2 provides, for two reasons. The pool alone owns the element, so no other path reaches the element. And no `&` in the element reaches program state that anything writes: a package constant is immutable, and a capability holds no state of the program. A call through a capability changes only capability-backed state, which §4.5 orders. A capability that the element got from a writable source may be written through the element, as [`effects.md`](effects.md) §6.2 permits; a capability that the element got from a read-only source stays read-only.

```zane
type Session = #struct {
    net &Net;          // a capability: allowed
    socket Socket;     // owned by the element
    seen Int;
}
```

An element is stored by the pool, as a `ListRef` element is stored by its list ([`generics.md`](generics.md) §8.5). A step call uses a fixed-size region of its own, with the same bounds as a spawned call's region, from when it begins until it returns, including any time it waits ([`memory.md`](memory.md) §3.7). Between calls, an element uses no fixed-size region.

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#a-worker-pools-element-may-reference-constants) — "A worker pool's element may reference constants".

### 6.5 The funnel is the one writer of the shared state

A step publishes an event by calling the function value it receives with the event, which copies the event into the funnel's queue. The function value is the same for every pool whose event type is `E`, and it captures nothing (§5.2), so it publishes to the pool whose work is running the call: a step or funnel call of that pool, or a call spawned from one. A program may store the function value like any other value, and a call to it publishes to the pool running that call, whichever pool the value came from. A call made where no pool of event type `E` is running it stops the program ([`packages.md`](packages.md) §6.3). The funnel applies each published event by being called with the shared state as `this` and the event, one call at a time. The funnel is the only writer of the shared state, and it never touches an element.

Events from one worker are applied in the order that worker published them. Events from different workers are applied in the order they arrive, which depends on scheduling (§3.7). The queue holding published events has no limit, so publishing never aborts and never waits.

A publish is a write of the pool's state, made when the funnel applies the event. The compiler never reorders a publish against the step's other writes or leaves it out, and never evaluates one at compile time under [`effects.md`](effects.md) §5.3. A pool created inside a `comptime` call runs at compile time with that call, like every other call it makes ([`effects.md`](effects.md) §5.6).

> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#workers-send-events-to-one-owner-workerpool) — "Workers send events to one owner: `WorkerPool`".
> **Story:** [`stories/concurrency.md`](../stories/concurrency.md#a-publish-goes-to-the-running-pool-and-missed-ticks-merge) — "A publish goes to the running pool, and missed ticks merge".

### 6.6 Reading a pool

```zane
S state(this @primitives$WorkerPool<T, S, E>)
@primitives$List<T> elements(this @primitives$WorkerPool<T, S, E>)
```

`state` returns a snapshot of the shared state. `elements` returns a list holding a snapshot of each element in the pool. Each element is coherent on its own, but the list as a whole is not one snapshot. `elements` is defined only for a value-typed `T`: the pool alone owns a reference-typed element, and a snapshot would copy it.

### 6.7 A pool's scope waits for its workers

A worker pool holds up the scope that owns it like a spawned call does (§4.1). The scope does not drain until every element's step has returned `false` and the funnel has applied every event published to it.

---

## 7. Summary

| Concept | Rule |
|---|---|
| Implicit parallelism | Compiler may parallelize only when results are unchanged |
| `spawn` | Starts a concurrent function or method call; blocks only when results are read; illegal on a verb taking a block argument |
| Abortable `spawn` | Must attach `?` or `??` directly to the spawn expression; the call settles once on the spawning thread, at the first read of its symbol or where its block ends, and the handler runs there |
| Water tower | A scope exits only after all spawned work completes, and every worker of a pool it owns has finished |
| Mutation | A spawned mutating call requires a value-typed subject, which is the only path by which a call writes caller-visible state, except that a root-package verb may write the program's console and runtime; at most one mutable borrow per storage location; the spawning scope writes no owner it lent a spawn until its block drains; a spawn in a loop body completes before the next pass; concurrent reads take a coherent snapshot |
| Snapshot of a boxed value | A value owning boxed members is snapshotted by a walk bounded at the scope's live block count — a depth no correct walk can reach, so exhaustion always indicates recycled bytes and stays retryable — that validates structure-directing metadata before typed interpretation and validates each handle's complete payload span — address, size, and alignment, inside a live region; the copy allocates in its destination binding's scope, rejected attempts return all provisional blocks before retrying, and a retry costs O(structure) |
| Worker pool | `@primitives$WorkerPool<T, S, E>` runs bulk work; `start` moves an element in and runs its step concurrently, again right away or when an event source is ready or the shared state changed, never twice at once for one element; a timer is ready at most once per step call, so missed intervals merge |
| Pool element | A value type, or a reference type whose every `&` names a package constant or a capability; a value is copied in and a reference-typed element moved in; a step call uses a fixed-size region from its start until it returns |
| Funnel | The one writer of a pool's value-typed shared state; applies published value-typed events one at a time, in order per worker; the queue has no limit, and a publish is an ordered write; a publish function publishes to the pool of its event type running the call, and stops the program where none is |
| Calls from a host | A host may call exported lambdas at once under the host rule: no two calls borrowing one C-held object mutably, and no call taking its owner beside another; an export writes only its subject and capability-backed state, reads every C-held object it borrows read-only as a snapshot, and has no second path to its subject |
| Conflicts on capability-backed state | Two accesses conflict only when they reach the same state: the console, the runtime, a foreign capability's own state, or one outside handle's |
