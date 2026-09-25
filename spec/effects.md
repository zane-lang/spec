# Zane Effect Model

This document specifies Zane's effect model: `mut`, read-only bindings, effect levels, capability access, structural inference, and the compiler guarantees built on top of those rules.

> **See also:** [`functions.md`](functions.md) §2 for method declarations. [`concurrency.md`](concurrency.md) §2 and §4 for parallelism and conflict rules. [`error-handling.md`](error-handling.md) §5 for the connection between effects and abortability.

---

## 1. Overview

Zane uses a structural effect model with a single user-facing effect modifier: `mut`.

- **`No purity keywords`.** Users do not write `pure`, `readonly`, or capability qualifiers.
- **`Subject-local mutation`.** `mut` grants write access to state reachable through `this`, including through guests.
- **`Read-only everywhere else`.** Every other parameter is read-only, and so is every guest derived from one. A `!` call is a write, exactly as an assignment is.
- **`Four effect levels`.** Whether a verb writes is declared by `mut`. Whether it reads capability-backed state, and whether it is proven to terminate, are derived from its body and the verbs it calls.
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

`mut` is the only effect modifier in the language. It appears on methods and grants write access to state reachable through `this`; the write lands on the caller's object or on state reachable from it. `this` is written bare for both kinds and carries no marker: a value-type `this` is a **borrow** of the caller's slot, and a reference-type `this` is an implicit **guest** to the object (see [`functions.md`](functions.md) §2.4). Neither takes hosting, so a `mut` call leaves the caller exactly as it found it.

### 2.4 Parameters are read-only

Parameters other than `this` are read-only (§4.1). A number parameter read in a body position resolves to a read-only number value ([`generics.md`](generics.md) §3.5).

> **Story:** [`stories/effects.md`](../stories/effects.md#where-mutation-is-allowed-to-reach) — "Where mutation is allowed to reach".

---

## 3. Effect Levels

Every verb has one of four effect levels: the strongest one required by any operation in its body or by any verb it calls transitively (§5.2). Three facts place it:

- **Writes.** A verb writes state its caller can observe only through `this` in a `mut` method (§4), or, in the root package, through the program's own capabilities (§6.6). Every other verb sits in one of the three levels that write nothing.
- **Reads.** A verb reads capability-backed state when it makes a `:` call on a capability, however the capability is reached, or calls a verb that does.
- **Termination.** Every repetition carries a count ([`control-flow.md`](control-flow.md) §3.5), so recursion is the only unbounded path. A verb terminates when no call path from it leads back to it and every callee is known (§5.4).

### 3.1 Level 1 — Total Pure

Total Pure functions depend only on explicit parameters and immutable package constants. They have no side effects and are guaranteed to terminate for all inputs.

### 3.2 Level 2 — Pure

Pure functions have no side effects but are not proven total. They are still reorderable and parallelizable at runtime, but they are not compile-time evaluated automatically.

### 3.3 Level 3 — Read-Only Impure

Read-Only Impure functions read capability-backed state but do not write.

### 3.4 Level 4 — Write Impure

Write Impure functions write through `this` in a `mut` method, or write capability-backed state.

> **Story:** [`stories/effects.md`](../stories/effects.md#a-mutating-call-is-a-write) — "A mutating call is a write".

> **Story:** [`stories/effects.md`](../stories/effects.md#four-levels-and-the-line-between-terminating-and-merely-pure) — "Four levels, and the line between terminating and merely pure".

---

## 4. Effect Enforcement

### 4.1 A read-only binding admits no write

A verb writes a place in one of two ways: it assigns to the place, or it calls a `mut` method with the place as the subject, and that method writes it. A read-only binding admits neither. The read-only bindings are every parameter other than `this`, and `this` in a method without `mut`. Everything reached through a read-only binding is read-only too: its fields, its elements, and the object each of its guests names.

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

Reading through a guest is not a side effect by itself. At use sites, guests follow the same field-access and method-call rules as hosts. Mutation of the hosted object's state must still be expressed through a `mut` method call with that object as the subject.

### 4.4 Read-only follows the guest

A guest derived from a read-only binding is read-only wherever it goes: bound to a local, stored in a field, passed as an argument, or returned. The compiler assumes a `mut` method may write through every guest its subject reaches. A `!` call is therefore a compile-time error when its subject reaches a read-only guest through any chain of fields and guests.

```zane
Unit f(console &Console) {
    k &Console = console;
    k!print("hi");        // ILLEGAL: k is derived from read-only console
    app App(console);     // App stores its argument in an `&` field
    app!run();            // ILLEGAL: app reaches a read-only guest
    return Unit();
}
```

Each verb judges this against its own bindings. Inside a verb, its parameters are read-only. At a call site, a guest the verb stores or returns takes the writability of the argument it came from, the same substitution that [`lifetimes.md`](lifetimes.md) §1.11 makes for owners. So `car!setEngine(engine)` leaves `car` writable when `engine` is the caller's own local, and makes `car` reach a read-only guest when `engine` is a parameter of the caller.

> **Story:** [`stories/effects.md`](../stories/effects.md#a-mutating-call-is-a-write) — "A mutating call is a write".

---

## 5. Structural Inference

### 5.1 Subject reachability drives effects

The compiler uses reachability from `this` to determine which state is writable in a `mut` method and readable in any method.

### 5.2 Call-graph propagation

If a function calls another function, its effect classification must be at least as strong as the called function's relevant effects.

### 5.3 Guests do not by themselves raise effect level

A function does not leave the pure levels merely because it reads through an `&`. Effect level is determined by the operations performed on the reachable object, not by whether the storage path is hosting or non-hosting.

### 5.4 Unknown callees are conservatively classified

If the compiler cannot prove the effect behavior of a callee, it must treat the call as requiring the strongest effect level needed to preserve safety.

> **Story:** [`stories/effects.md`](../stories/effects.md#inferring-effects-instead-of-naming-them) — "Inferring effects instead of naming them".

---

## 6. Capability Wiring and Explicit State Flow

### 6.1 Capabilities must be passed or stored explicitly

There is no ambient global I/O capability. Code can reach external state only through capability objects it receives or holds. A capability received as a parameter can be read (§4.1). Writing one takes a `mut` method whose `this` reaches it. The one source of capabilities is `@program$`, which only the root package reaches (§6.6); everything else receives them from there.

### 6.2 Constructor injection is ordinary capability wiring

Capabilities may be stored into objects at construction time. This does not create ambient authority; it only records an explicit hosting path by which later methods can reach the capability. A capability stored from a writable source can be written by the object's `mut` methods; one stored from a read-only source stays read-only (§4.4).

### 6.3 `&` fields can also expose read access paths

Storing an `&` field is another explicit way to make state reachable. This does not create a distinct use-site effect rule; the effect level still comes from what the reachable operations do.

### 6.4 Context objects are explicit, not magical

A "context object" that groups several capabilities is just another ordinary object in the hosting graph. It may reduce parameter count, but it does not hide effects from the compiler because the reachable capabilities are still explicit in storage and call structure.

### 6.5 Prop drilling is intentional

Passing capabilities through constructors and methods is part of the design. It keeps effects visible in object structure rather than hidden in ambient module state.

> **Story:** [`stories/effects.md`](../stories/effects.md#no-ambient-io-effects-you-can-see-in-the-structure) — "No ambient I/O: effects you can see in the structure".

### 6.6 The program's console and runtime

The console and the runtime are capabilities the compiler supplies. Their types, `@runtime$Console` and `@runtime$Runtime`, are reference types: a program has one console and one runtime, and every part of it that uses either uses the same one. Neither type can be constructed. The only instances are `@program$console` and `@program$runtime`, created when the program starts.

Only the root package reaches `@program$` ([`packages.md`](packages.md) §6.1). It passes the instances on like any other capability. Passed as an argument, a capability can be read. Stored into an object at construction, it can be written by that object's `mut` methods (§4.4). Either way, a package that prints or configures the runtime shows it in what it receives (§6.1, §6.5). Every package can name the types, which is what lets a verb declare a parameter or field of either.

Their methods are stated over storage primitives, like every intrinsic, and are found through the type's home, `@runtime$` ([`functions.md`](functions.md) §6.1). `std` wraps the console in its own `Console`, whose methods take `String`:

```zane
console Console(@program$console);
console!print("hello world");
```

Writing to the console and changing the runtime's configuration are writes to capability-backed state, so a verb that does either is Write Impure (§3.4).

> **Story:** [`stories/effects.md`](../stories/effects.md#where-the-first-capability-comes-from) — "Where the first capability comes from".

---

## 7. Constructors, Allocation, and Abortability

### 7.1 Constructors may allocate but are not `mut`

Constructors create values and therefore participate in allocation, but they do not mutate an existing subject.

### 7.2 Allocation and destruction do not by themselves raise effect level

Heap allocation and destruction are runtime implementation events, but they are not side effects by themselves for effect classification. A function stays in the pure levels unless it also mutates subject-reachable state or reads/writes through capabilities.

### 7.3 Abortability is orthogonal

A function's abort type and effect level are independent. An abortable function may be Total Pure, Read-Only Impure, or Write Impure depending on what else it does.

> **Story:** [`stories/effects.md`](../stories/effects.md#what-deliberately-is-not-an-effect) — "What deliberately is not an effect".

---

## 8. Concurrency Implications

### 8.1 Total Pure and Pure work are natural parallelization candidates

Because they do not write mutable state, they can be reordered and parallelized subject to profitability heuristics.

### 8.2 Reads compose with concurrent mutation

Multiple concurrent reads are legal. For external, capability-backed state a read that conflicts with a concurrent write is serialized by the compiler/runtime. For in-memory value state, a concurrent read instead takes a coherent snapshot rather than blocking (see [`concurrency.md`](concurrency.md) §4.4).

### 8.3 Concurrent mutation is governed by the spawn rules

Concurrent mutation is not a per-`mut`-call property; it is governed by the spawn rules in [`concurrency.md`](concurrency.md) §4. A spawned mutating call's subject **MUST** be a value type, and no two concurrent spawns may mutably borrow the same storage — including two iterations of one spawn site inside a loop. A value type's transitive alias-freedom (see [`memory.md`](memory.md) §2.10) is what lets the compiler settle the absence of a data race from the subject's type alone.

---

## 9. Effect Level Matrix

| Level | Reads capability-backed state | Writes subject-reachable state | May write external state | Compile-time evaluation |
|---|---|---|---|---|
| Total Pure | ❌ | ❌ | ❌ | ✅ |
| Pure | ❌ | ❌ | ❌ | ❌ |
| Read-Only Impure | ✅ | ❌ | ❌ | ❌ |
| Write Impure | ⚠️ may | ✅ possible | ⚠️ may | ❌ |
