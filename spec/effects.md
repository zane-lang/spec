# Zane Effect Model

This document specifies Zane's effect model: `mut`, read-only bindings, what each kind of verb may write, capability access, and the compiler guarantees built on top of those rules.

> **See also:** [`functions.md`](functions.md) §2 for method declarations. [`concurrency.md`](concurrency.md) §2 and §4 for parallelism and conflict rules. [`error-handling.md`](error-handling.md) §5 for the connection between effects and abortability.

---

## 1. Overview

Zane uses a structural effect model with a single user-facing effect modifier: `mut`.

- **`No purity keywords`.** Users do not write `pure`, `readonly`, or capability qualifiers.
- **`Subject-local mutation`.** `mut` grants write access to state reachable through `this`, including through references.
- **`Read-only everywhere else`.** Every other parameter is read-only, and so is every reference derived from one. A `!` call is a write, exactly as an assignment is.
- **`Three kinds of verb`.** A `mut` method writes `this`. A method without `mut` and a function write nothing their caller can see. The signature says which.
- **`Capability-based external effects`.** I/O and external state remain explicit because capability objects must be passed or stored. They originate in `@program$`, which only the root package reaches.

> **Story:** [`stories/effects.md`](../stories/effects.md#inferring-effects-instead-of-naming-them) — "Inferring effects instead of naming them".

---

## 2. Core Definitions

### 2.1 Side effect

A side effect is any observable interaction beyond returning a value, including:

- writing through `this`
- interacting with capability objects

### 2.2 Capability

A capability is an object whose methods model access to external state, such as a filesystem, logger, socket, clock, or random source.

### 2.3 `mut`

`mut` is the only effect modifier in the language. It appears on methods and grants write access to state reachable through `this`; the write lands on the caller's object or on state reachable from it. `this` is written bare for both kinds and carries no marker: it is a **borrow** of the caller's value or owner (see [`functions.md`](functions.md) §2.4). It never takes ownership: a `mut` call may change the caller's object, but the caller still holds it afterwards.

### 2.4 Parameters are read-only

Parameters other than `this` are read-only (§4.1). A number parameter read in a body position resolves to a read-only number value ([`generics.md`](generics.md) §3.5).

> **Story:** [`stories/effects.md`](../stories/effects.md#where-mutation-is-allowed-to-reach) — "Where mutation is allowed to reach".

---

## 3. What Each Kind of Verb May Write

A verb's signature says what it may write. There are three kinds:

| Verb | Called with | May write |
|---|---|---|
| `mut` method | `!` | `this` and everything reached through it (§4.2) |
| Method without `mut` | `:` | nothing its caller can see |
| Function | a plain call | nothing its caller can see |

Every verb may also write storage it owns itself, which its caller never sees. Whatever a verb calls stays within the same bound: a callee writes only through its own `this`, and the caller supplies that `this` with a `!` call on something the caller may itself write (§4). So a verb's kind bounds everything the call can write, however deep the calls go.

The root package is the one exception. Any verb there may write the program's console and runtime (§6.6).

Any verb may read capability-backed state through a `:` call on a capability it reaches. A read is not a write, so it does not change what the verb may write. It matters only for ordering against concurrent writes ([`concurrency.md`](concurrency.md) §4.5) and for what the compiler may evaluate ahead of time (§5.3).

> **Story:** [`stories/effects.md`](../stories/effects.md#a-mutating-call-is-a-write) — "A mutating call is a write".

---

## 4. Effect Enforcement

### 4.1 A read-only binding admits no write

A verb writes a place in one of two ways: it assigns to the place, or it calls a `mut` method with the place as the subject, and that method writes it. A read-only binding admits neither. The read-only bindings are every parameter other than `this`, and `this` in a method without `mut`. Everything reached through a read-only binding is read-only too: its fields, its elements, and the object each of its references names.

```zane
Unit report(console &Console, msg String) {
    console!print(msg);   // ILLEGAL: console is read-only
    return Unit();
}

Unit log(this Console, msg String) mut {
    this!print(msg);      // legal: the subject of a `mut` method
    return Unit();
}

console!log("hello");
```

### 4.2 `mut` does not authorize arbitrary writes

`mut` makes `this` writable and nothing else: a `mut` method's other parameters stay read-only (§4.1). This applies whether the subject is a value type or a reference type: a value subject is mutated in place through its borrow (see [`functions.md`](functions.md) §2.4), not by returning a replacement.

### 4.3 `&` use sites follow ordinary call rules

Reading through a reference is not a side effect by itself. At use sites, references follow the same field-access and method-call rules as owners. Mutation of the owned object's state must still be expressed through a `mut` method call with that object as the subject.

### 4.4 Read-only follows the reference

A reference derived from a read-only binding is read-only wherever it goes: bound to a local, stored in a field, passed as an argument, or returned. The compiler assumes a `mut` method may write through every reference its subject reaches. A `!` call is therefore a compile-time error when its subject reaches a read-only reference through any chain of fields and references.

```zane
Unit f(console &Console) {
    k &Console = console;
    k!print("hi");        // ILLEGAL: k is derived from read-only console
    app App(console);     // App stores its argument in an `&` field
    app!run();            // ILLEGAL: app reaches a read-only reference
    return Unit();
}
```

Each verb judges this against its own bindings. Inside a verb, its parameters are read-only. At a call site, a reference the verb stores or returns takes the writability of the argument it came from, the same substitution that [`lifetimes.md`](lifetimes.md) §1.11 makes for scopes. So `car!setEngine(engine)` leaves `car` writable when `engine` is the caller's own local, and makes `car` reach a read-only reference when `engine` is a parameter of the caller.

> **Story:** [`stories/effects.md`](../stories/effects.md#a-mutating-call-is-a-write) — "A mutating call is a write".

---

## 5. What the Compiler Derives

### 5.1 Subject reachability drives effects

The compiler uses reachability from `this` to determine which state is writable in a `mut` method and readable in any method.

### 5.2 Capability access

Two facts about a call are not in the signature: whether the call reads capability-backed state, and whether it writes it (§3, §6.6). The compiler derives both from the verb's body and from every verb it calls, and uses them only to decide what it may evaluate at compile time, which turns on reads alone (§5.3), and what it may run in parallel, which turns on both ([`concurrency.md`](concurrency.md) §2).

### 5.3 Compile-time evaluation

A verb receives what it works with through its parameters and through capability-backed state; everything else it uses is a literal, a constant, or computed from them. So the compiler can tell which computations depend on neither a parameter nor a read of capability-backed state, and it may evaluate any of them at compile time. It may also leave any of them for run time. Evaluating one at compile time changes none of the program's side effects (§2.1) and nothing it computes: each write the computation makes still happens at run time, at the same point and in the same order. Only how long the program takes to run changes.

```zane
Int loud(n Int) {
    console!print("computing\n");
    return n * 2;
}

answer Int = loud(21); // may be evaluated at compile time; "computing" is still printed, before answer is set
```

> **Story:** [`stories/effects.md`](../stories/effects.md#compile-time-evaluation-needs-no-termination-proof-and-sits-beside-capability-access) — "Compile-time evaluation needs no termination proof and sits beside capability access".

### 5.4 Reading through a reference is a read

Reading through an `&` is a read like any other. What a verb may write follows from its kind (§3), not from whether it reaches an object through an owner or a reference.

### 5.5 Unknown callees are assumed to touch capabilities

When the compiler cannot see a callee's body, as with a call through a function value it cannot resolve, it assumes the call reads and writes capability-backed state.

> **Story:** [`stories/effects.md`](../stories/effects.md#inferring-effects-instead-of-naming-them) — "Inferring effects instead of naming them".

---

## 6. Capability Wiring and Explicit State Flow

### 6.1 Capabilities must be passed or stored explicitly

There is no ambient global I/O capability. Code can reach external state only through capability objects it receives or holds. A capability received as a parameter can be read (§4.1). Writing one takes a `mut` method whose `this` reaches it. The one source of capabilities is `@program$`, which only the root package reaches (§6.6); everything else receives them from there.

### 6.2 Constructor injection is ordinary capability wiring

Capabilities may be stored into objects at construction time. This does not create ambient authority; it only records an explicit owning path by which later methods can reach the capability. A capability stored from a writable source can be written by the object's `mut` methods; one stored from a read-only source stays read-only (§4.4).

### 6.3 `&` fields can also expose read access paths

Storing an `&` field is another explicit way to make state reachable. This does not create a distinct use-site effect rule; what a verb may write through it still follows from the verb's kind (§3).

### 6.4 Context objects are explicit, not magical

A "context object" that groups several capabilities is just another ordinary object in the ownership graph. It may reduce parameter count, but it does not hide effects from the compiler because the reachable capabilities are still explicit in storage and call structure.

### 6.5 Prop drilling is intentional

Passing capabilities through constructors and methods is part of the design. It keeps effects visible in object structure rather than hidden in ambient module state.

> **Story:** [`stories/effects.md`](../stories/effects.md#no-ambient-io-effects-you-can-see-in-the-structure) — "No ambient I/O: effects you can see in the structure".

### 6.6 The program's console and runtime

The console and the runtime are capabilities the compiler supplies. Their types, `@runtime$Console` and `@runtime$Runtime`, are reference types: a program has one console and one runtime, and every part of it that uses either uses the same one. Neither type can be constructed. The only instances are `@program$console` and `@program$runtime`, created when the program starts.

Only the root package reaches `@program$` ([`packages.md`](packages.md) §6.1). It passes the instances on like any other capability. Passed as an argument, a capability can be read. Stored into an object at construction, it can be written by that object's `mut` methods (§4.4). Either way, a package that prints or configures the runtime shows it in what it receives (§6.1, §6.5). Every package can name the types, which is what lets a verb declare a parameter or field of either.

Their methods are stated over storage primitives, like every intrinsic, and are found through the type's home, `@runtime$` ([`functions.md`](functions.md) §6.1). `core` wraps the console in its own `Console`, whose methods take `String`:

```zane
console Console(@program$console);
console!print("hello world");
```

The console's own method borrows a string primitive ([`types.md`](types.md) §2.7). It writes exactly its length in bytes and does not interpret escapes, so the bytes need no terminator:

```zane
@primitives$Unit print(this @runtime$Console, text @primitives$String) mut
```

The runtime's `arguments` method gives the arguments the program was started with:

```zane
^@primitives$List<@primitives$String> arguments(this @runtime$Runtime)
```

It returns a new list holding one string per argument, in the order they were given, without the program's own name. Where the system passes arguments as bytes, each string holds those bytes unchanged. Where it passes them as UTF-16, each string holds their UTF-8 encoding. A program started with no arguments gets an empty list.

Writing to the console and changing the runtime's configuration are writes to capability-backed state. In the root package any verb may make them (§3). Elsewhere only a `mut` method whose `this` reaches the console or runtime can. Calling `arguments` is a read of capability-backed state, so any verb that reaches the runtime may make it, and no computation that depends on what it returns is evaluated at compile time (§5.3).

> **Story:** [`stories/effects.md`](../stories/effects.md#where-the-first-capability-comes-from) — "Where the first capability comes from".
> **Story:** [`stories/effects.md`](../stories/effects.md#the-console-moves-into-core) — "The console moves into `core`".

---

## 7. Constructors, Allocation, and Abortability

### 7.1 Constructors may allocate but are not `mut`

Constructors create values and therefore participate in allocation, but they do not mutate an existing subject.

### 7.2 Allocation and destruction are not writes

Heap allocation and destruction are runtime implementation events, not writes in the sense of §4.1. A verb that allocates and destroys only its own storage writes nothing its caller can see.

### 7.3 Abortability is orthogonal

A verb's abort type and its kind (§3) are independent: a verb of any kind may be abortable.

> **Story:** [`stories/effects.md`](../stories/effects.md#what-deliberately-is-not-an-effect) — "What deliberately is not an effect".

---

## 8. Concurrency Implications

### 8.1 Work that writes nothing is a natural parallelization candidate

A call to a method without `mut` or to a function writes nothing its caller can see (§3). Unless it touches capability-backed state (§5.2), the compiler may reorder it and run it in parallel, subject to profitability heuristics.

### 8.2 Reads compose with concurrent mutation

Multiple concurrent reads are legal. For external, capability-backed state a read that conflicts with a concurrent write is serialized by the compiler/runtime. For in-memory value state, a concurrent read instead takes a coherent snapshot rather than blocking (see [`concurrency.md`](concurrency.md) §4.4).

### 8.3 Concurrent mutation is governed by the spawn rules

Concurrent mutation is not a per-`mut`-call property; it is governed by the spawn rules in [`concurrency.md`](concurrency.md) §4. A spawned mutating call's subject **MUST** be a value type, and no two concurrent spawns may mutably borrow the same storage — including two iterations of one spawn site inside a loop. A value type's transitive alias-freedom (see [`memory.md`](memory.md) §2.10) is what lets the compiler settle the absence of a data race from the subject's type alone.

---

## 9. Summary

| Concept | Rule |
|---|---|
| `mut` method | Called with `!`; writes `this` and everything reached through it |
| Method without `mut`, function | Write nothing their caller can see |
| Read-only binding | Every parameter other than a `mut` method's `this`; admits neither an assignment nor a `!` call |
| Derived reference | A reference derived from a read-only binding stays read-only wherever it goes; a `!` call whose subject reaches one is an error |
| Root package | Any verb may write the program's console and runtime |
| Derived facts | Capability reads and capability writes come from the body and its callees, and govern only compile-time evaluation and parallelism |
| Compile-time evaluation | A computation that depends on no parameter and no read of capability-backed state may be evaluated at compile time, which changes none of the program's side effects and nothing it computes |
| Capabilities | Reachable only as objects passed or stored; they originate in `@program$` |
| Program arguments | Read through the runtime's `arguments`; nothing that depends on them is evaluated at compile time |
