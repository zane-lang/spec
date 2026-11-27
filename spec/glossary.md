# Zane Terminology

This document records the canonical names used across the Zane specification for recurring language concepts and design rules. It does not add semantics; each term points back to the document that defines the rule in full and explains why that label is the preferred name.

> **See also:** [`README.md`](README.md) for the document index. [`syntax.md`](syntax.md) for canonical surface forms. Topic documents for the full semantic rules behind each term.

---

## 1. How to Use This File

This file gives short, reusable names to concepts that appear across multiple spec documents.

- **`Preferred label`.** Each entry records the name the spec should reuse when the same concept appears again.
- **`Preferred casing`.** Terms keep the casing that best matches their role in the spec: formal named models may use capitals, while ordinary reusable noun phrases may stay lowercase.
- **`Meaning`.** Each entry gives only a short summary, not the full rule.
- **`Why this name`.** Each entry explains the connection between the label and the underlying rule.
- **`Canonical home`.** Each entry names the document section where the full rule is specified.

---

## 2. Error Handling, Control Flow, and Concurrency

### 2.1 Bifurcated Return Path

- **Meaning:** An abortable call has a statically typed primary path and a statically typed abort path.
- **Why this name:** The rule splits one call result into two explicit return paths instead of hiding failure in a side channel.
- **Canonical home:** [`error-handling.md`](error-handling.md) §1

### 2.2 resolve-only shorthand

- **Meaning:** `??` desugars to a `?` handler that only provides a fallback `resolve`.
- **Why this name:** The form is a shorthand for the subset of handler behavior that resolves a replacement value and does nothing else.
- **Canonical home:** [`error-handling.md`](error-handling.md) §3.3

### 2.3 call exit

- **Meaning:** `@controlflow$exitFromCall()` ends the invocation that called the verb whose body contains it, reaching one level and no further. That is what lets an exit be a declared verb — `core`'s `guard` — rather than grammar, since a verb built on it exits whoever calls it. A lambda's body may not contain it.
- **Why this name:** What the exit ends is the call it was reached from, not its own frame.
- **Canonical home:** [`control-flow.md`](control-flow.md) §4.2

### 2.4 value-typed mutation rule

- **Meaning:** A spawned call may mutate only a value-typed subject, and at most one live spawn may mutably borrow a given storage location; a worker pool's element may also be a reference type whose every `&` names a constant. A value type is transitively alias-free, so the rule rules out an aliased data race from the subject's type alone; concurrent reads take a coherent snapshot instead of serializing.
- **Why this name:** Concurrent mutation is gated on the subject being a value type — the property that makes race-freedom checkable without whole-program alias analysis.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.2, §4.3, and §6.4

### 2.5 water-tower lifetimes

- **Meaning:** Objects owned in a scope stay alive until every `spawn`ed call in that scope has completed, and every worker of a pool it owns has finished, and the scope drains. A pool's element is the exception: it dies when its own step returns `false`.
- **Why this name:** The source document explains the rule through a water-tower analogy in which each still-running spawned call acts like a plate holding the water level up.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.1

### 2.6 structural effect model

- **Meaning:** What a verb may write follows from its kind — a `mut` method writes `this`, a method without `mut` or a function writes nothing its caller can see — and capability access follows from call structure, with no effect annotations.
- **Why this name:** The model is "structural" because effects follow from program structure and reachable state, not from separate effect declarations.
- **Canonical home:** [`effects.md`](effects.md) §1, §3, and §5

### 2.7 capability wiring

- **Meaning:** Capability objects must be passed or stored explicitly so access to external state remains visible in the object graph and call graph. They originate in the root package and in exported lambdas, from `@program$` and the foreign variables they declare.
- **Why this name:** The design treats capabilities like explicit wiring between components rather than ambient globals.
- **Canonical home:** [`effects.md`](effects.md) §6

### 2.8 1-based ordinal counting

- **Meaning:** Counted repetition and positional indexing start at `1`, so an ordered sequence's final valid position is its size. A position outside the sequence stops the program.
- **Why this name:** The term makes the rule about ordinal positions explicit and distinguishes it from raw numeric arithmetic.
- **Canonical home:** [`control-flow.md`](control-flow.md) §5

### 2.9 worker / step

- **Meaning:** A repetition with no written bound: a subject and a step, a non-capturing `Bool[this S] mut` function value that runs on that subject until it returns `false`. `core`'s `work`, `register`, and `setInterval` run one, over `@controlflow$work` and `@controlflow$register`; none takes a block, so a program may spawn a worker.
- **Why this name:** It is a unit of ongoing work with state of its own, which is what separates it from a loop, a step of the program that contains it; `step` is the single advance that the worker repeats.
- **Canonical home:** [`control-flow.md`](control-flow.md) §3.7

### 2.10 worker pool / element

- **Meaning:** `@primitives$WorkerPool<T, S, E>`, which runs bulk work: each worker is an **element** that `start` copies in, or moves in when it is a reference type, and a step the runtime calls again right away, or when an event source is ready or the shared state changed. Workers share a value-typed state that only the funnel writes.
- **Why this name:** A pool holds many workers in the sense of [`control-flow.md`](control-flow.md) §3.7, each a subject and its step, and "pool" is the usual word for many interchangeable workers; "element" is the standard term for an item that a container holds.
- **Canonical home:** [`concurrency.md`](concurrency.md) §6

### 2.11 funnel

- **Meaning:** A worker pool's one writer of its shared state. It applies the value-typed events the pool's steps publish, each copied into its queue, one at a time, in order per worker, and never touches an element.
- **Why this name:** Many workers pour events in and one outlet applies them, the shape of a funnel; "handler" already names the `?` abort handler.
- **Canonical home:** [`concurrency.md`](concurrency.md) §6.5

### 2.12 event source

- **Meaning:** An `@primitives$EventSource`, such as a timer, that a worker waits on between calls of its step: a worker pool's worker is started with event sources, and `register` runs a worker on one. The step runs again when a source is ready, and while the worker waits, every other spawned call and pool worker keeps running.
- **Why this name:** It is the standard term in event loops for something that becomes ready; the qualifier keeps it apart from a reference source.
- **Canonical home:** [`concurrency.md`](concurrency.md) §6.3

---

## 3. Types, Storage, and Binding

### 3.1 place expression

- **Meaning:** A place expression denotes an existing storage location. Reference-source eligibility is separate: not every place may mint a new `&` (§3.36).
- **Why this name:** The term names the expressions that refer to a storage "place" rather than to a temporary value.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.2 value-downstream enforcement

- **Meaning:** A value type may contain only value types, value-type primitives among them, never a reference-type or `&` field anywhere downstream in nested value-type fields. The rule turns on copying — a value is copied, and a reference type exists in order not to be. It does **not** bar recursion: a value type may lead back to itself through a boxed member (§3.39).
- **Why this name:** The rule is checked recursively through fields downstream from the outer value type, not just at the first field layer.
- **Canonical home:** [`memory.md`](memory.md) §2.10

### 3.3 unified type parameters

- **Meaning:** A **type parameter** (`name Type`, uppercase, ranging over types) or a **number parameter** (`name @concepts$Int`, lowercase, ranging over compile-time integers). Both share one reference system — bare names, with casing carrying the kind — and differ only in where they are introduced: a `<>` header on a type, inline on a verb.
- **Why this name:** Type and number parameters share one concept-and-reference system (the `Type`/`@concepts$Int` concepts, bare references, and the casing rule) across types and verbs; only the introduction site differs — a header for types, which are applied positionally, and inline for verbs, whose parameters are always inferred.
- **Canonical home:** [`generics.md`](generics.md) §3

### 3.4 compiler concept types

- **Meaning:** Compiler-provided types for source constructs, permitted in parameters but not storage. The leaf concepts `Type`, `@concepts$Int`, and `@concepts$Float` are compile-time values. String concepts may contain runtime interpolation; array and map concepts may contain runtime entries. A concept is not necessarily a compile-time value.
- **Why this name:** These are compiler-defined concept-level placeholders for source literals, not ordinary user storage types. Each literal concept is named with the term that other languages usually use for that literal: `@concepts$Int` is an integer literal, whichever integer primitive's constructor it is passed to.
- **Canonical home:** [`syntax.md`](syntax.md) §2.8–§2.12; lowering in [`types.md`](types.md) §2.7

### 3.5 field constructor

- **Meaning:** Constructor syntax may declare fields directly in the parameter header and map them into `init{}`.
- **Why this name:** The written constructor header is shaped around fields themselves rather than around separate parameter names.
- **Canonical home:** [`types.md`](types.md) §3.3

### 3.6 package privacy

- **Meaning:** A `_` field is private to the package that declares its type: any declaration in that package may name it, and none in another package may, including methods on the type.
- **Why this name:** The package is the boundary, the same one a leading `_` draws on a package-scope declaration.
- **Canonical home:** [`types.md`](types.md) §2.3

### 3.7 direct initialization

- **Meaning:** Every symbol declaration provides its initial value in the declaration itself; bare declarations without an initializer are illegal.
- **Why this name:** The rule is about initialization happening directly at the binding site, not later through control flow.
- **Canonical home:** [`memory.md`](memory.md) §2.11

### 3.8 call-only callable

- **Meaning:** Methods, functions, and operators may appear only in call position; they have no value form and cannot be referenced as values.
- **Why this name:** The name states the single permitted use site — a call — and contrasts it with the value form that callables deliberately lack.
- **Canonical home:** [`functions.md`](functions.md) §7.1

### 3.9 lambda-variable

- **Meaning:** A symbol bound to a lambda literal. It has one function type and is the only way to hold a function value, since callables themselves are call-only.
- **Why this name:** The term pairs the lambda value with the variable that names it, distinguishing it from an anonymous lambda literal and from a call-only callable.
- **Canonical home:** [`functions.md`](functions.md) §7.3

### 3.10 types as templated functions

- **Meaning:** A type definition takes parameters and is executed to produce a concrete layout, the way a function takes parameters and produces a value. Templating is a direct consequence of types being executable rather than a separate feature.
- **Why this name:** The label states the model directly: a type is a function over its parameters, and applying arguments evaluates it into a concrete type.
- **Canonical home:** [`generics.md`](generics.md) §2

### 3.11 type expression vs constructor call

- **Meaning:** `Type<...>` is a compile-time type expression that applies arguments to a parameterized type and describes architecture; `Type(...)` is a runtime constructor call that builds a value. A constructor call is always by bare name and never carries a `<>` list.
- **Why this name:** The two forms mention the same type name but belong to different systems — the type system versus the value system — so the contrast names the boundary.
- **Canonical home:** [`generics.md`](generics.md) §4 and §5

### 3.12 distinct type vs alias

- **Meaning:** `type Name = T` introduces a new distinct type that is structurally equal to `T` but not interchangeable with it; `alias Name = T` introduces a fully interchangeable name. The keyword, `type` or `alias`, selects between the two.
- **Why this name:** The pairing names the only difference between the two declaration forms — whether the result is a new type or just another name.
- **Canonical home:** [`types.md`](types.md) §5

### 3.13 casing-determined kind

- **Meaning:** The first letter of an identifier selects its lexical class: an uppercase-initial identifier is a type, and a lowercase-initial identifier is a value, binding, or parameter. A lowercase name in a type position is a compile-time error.
- **Why this name:** Casing alone, not a declaration or lookahead, determines whether a bare name is a type or a value.
- **Canonical home:** [`lexical.md`](lexical.md) §3

### 3.14 `Type` and `@concepts$Int` parameter concepts

- **Meaning:** `Type` and `@concepts$Int` are compiler concept types used to declare type and number parameters (`T Type`, `n @concepts$Int`) — the only two kinds a type's `<>` header holds. Like other concept types they are legal only in parameter positions, never as storage. As `()` value parameters they are passed explicitly; introduced inline on a verb parameter's type, in a nested type, or in a slot of a verb parameter's function type, they are inferred; listed in a type's `<>` header they are applied positionally at use sites.
- **Why this name:** A type or size handed to a declaration is a compile-time value, so its parameter has a concept type like any other — for a size, the concept that an integer literal already carries — rather than a bespoke parameter-kind keyword.
- **Canonical home:** [`generics.md`](generics.md) §3.3

### 3.15 variant (sum mould)

- **Meaning:** A `variant` is a **sum mould**: a value of the type it declares holds exactly one of its named members at a time. Its body grammar is identical to a `struct` — the product mould — with the keyword flipping product into sum. Reading a member is partial and therefore abortable. A variant value is built by naming a case, or by a constructor the variant declares, which returns a case.
- **Why this name:** "Variant" is the established name for a tagged sum of alternatives, and it reads as a peer of `struct` since the two share one body grammar.
- **Canonical home:** [`adt.md`](adt.md) §3

### 3.16 enum (uniform peers)

- **Meaning:** An `enum` is a closed set of interchangeable, payloadless peer members that mean one uniform thing (colors, weekdays). It is not a sum mould; per-member data is attached externally by an enum map. An enum may declare constructors, each returning a member.
- **Why this name:** "Enum" matches the common meaning of an enumeration of equal-rank constants, and the spec reserves it for that uniform-peer role rather than overloading it with the sum-type role given to `variant`.
- **Canonical home:** [`adt.md`](adt.md) §2

### 3.17 struct/variant body symmetry

- **Meaning:** A `struct` body and a `variant` body use the exact same grammar; the keyword alone determines whether the type is a product or a sum. The symmetry applies to the declaration, not to consuming code, where construction and reads differ.
- **Why this name:** The label states the shared property directly: one body shape serves both kinds, distinguished only by keyword.
- **Canonical home:** [`adt.md`](adt.md) §3.1

### 3.18 variant matching

- **Meaning:** Consuming a `variant` by dispatching on its live tag in a central `match` block and binding the payload whole. It is **not** pattern matching: it does not destructure payload shape, nest into inner variants, test literals, or apply guards. A `[ ]` group in an arm selects a set of tags, not a shape.
- **Why this name:** It matches a variant's tag, distinguishing it from ML-style pattern matching, which also destructures shape.
- **Canonical home:** [`adt.md`](adt.md) §5.3

### 3.19 `match`

- **Meaning:** An expression, legal anywhere an expression is accepted, that names a scrutinee and a `{ }` block of `;`-terminated arms; each arm has an optional binder, a case (or `[ ]` group of cases) selector, and a body. It dispatches on the live tag, is exhaustive with no default arm, all arms share one return type, and abort flows through.
- **Why this name:** "Match" is the familiar name for tag-directed selection, here surfaced as a single central block over a variant's cases.
- **Canonical home:** [`adt.md`](adt.md) §5

### 3.20 enum map property

- **Meaning:** A package-scope, exhaustive, access-only declaration that attaches uniform external data to an enum's members and is read field-style (`Colors.red.colorName`). It is not a passable value; its result is a value.
- **Why this name:** It maps each enum member to a value of a named property, and it is named where the value is read, so "enum map property" describes both the table and its access form.
- **Canonical home:** [`adt.md`](adt.md) §6

### 3.21 bracket-picked separator

- **Meaning:** The bracket determines how its contents are separated: a `{ }` terminates each with `;` — entries of a body, always trailing, and statements of a code block alike, save a statement a `}` already ends, a rule package-scope declarations follow too; a `[ ]`, `( )`, or `< >` list separates its entries with `,`, never trailing.
- **Why this name:** The rule turns on the bracket alone rather than on what the construct means, so the name states that the bracket does the picking.
- **Canonical home:** [`lexical.md`](lexical.md) §6

### 3.22 verb

- **Meaning:** A callable whose body is a sequence of statements that executes to do work. The verbs are functions, methods, operators, constructors, and lambdas (a lambda being an anonymous verb, the only verb that also has a value form). A subscript is **not** a verb: its body must be a place expression that projects existing storage rather than running computation, so it designates a place instead of executing.
- **Why this name:** The unifying trait is the executing statement body — a verb *does* something — which is why a constructor (statements ending in a `return` of its type's value, an `init{}` for a `struct`) counts and is indistinguishable from a builder helper apart from its name, while a place-projecting subscript does not.
- **Canonical home:** [`functions.md`](functions.md) §1

### 3.23 settled owner

- **Meaning:** An owner that may be referenced and never moves again: a bare reference-type symbol, or a field or `ArrayRef` element of a settled root. A roaming owner settles by moving into a settled place. Overwriting it writes the replacement at the same address.
- **Why this name:** A settler has stopped travelling and taken a fixed home; a reference can name only an owner that has done so, and the word "settled" marks an owner that has stopped moving for good.
- **Canonical home:** [`memory.md`](memory.md) §2.1

### 3.24 roaming owner

- **Meaning:** An owner that may move anywhere. Neither it nor anything inside it can be referenced. It is a symbol, parameter, return, or abort written `^T` with `T` a reference type, a field or `ArrayRef` element of a roaming root, or any `ListRef` element or variant payload.
- **Why this name:** The opposite of *settled* in the same register: an owner still travelling, which no reference can name.
- **Canonical home:** [`memory.md`](memory.md) §2.1

### 3.25 arena placement

- **Meaning:** Where an owned object's storage is materialized: the arena of the scope that creates it, split into a **fixed-size** region for statically sized slots and the handles that sit in them, and a **dynamic** region for the payloads those handles name. Placement is an unobservable implementation choice.
- **Why this name:** Placement is a choice among **arenas** — the per-scope regions — rather than between a stack and a heap; the creating scope's arena is the default, a parent arena the fallback on escape.
- **Canonical home:** [`memory.md`](memory.md) §3.5

### 3.26 capability marker

- **Meaning:** A surface marker on a verb that selects its kind and unlocks one capability: naming the first parameter `this` makes a method and grants the subject call; naming the verb after a type makes a constructor, implying its return type and, for a type with fields, unlocking `init{ }`; naming the verb with an operator token makes an operator; no name makes a lambda. The parameter system, body grammar, overload resolution, and effect model are shared across all verbs.
- **Why this name:** The marker is a small piece of surface form that, by its presence, grants a *capability* to an otherwise-ordinary verb — so a constructor is a verb with one marker, not a separate mechanism.
- **Canonical home:** [`functions.md`](functions.md) §8

### 3.27 borrow

- **Meaning:** Non-owning, non-escaping access to a caller's storage for the duration of a call: a bare parameter of either kind of type, and every subject. It is the only way a value type is passed. A borrow is mutable only as a `mut` subject, nothing else in the call writes the place it lends, and a value is copied only when bound into a fresh slot.
- **Why this name:** The callee is lent the caller's storage for the call and gives it back at return — it does not own it and cannot keep it. Unlike a reference, the borrow itself cannot be stored, returned, or used as a move-source — a restriction on the borrow, not on the value read through it, which a value type may still copy into a fresh slot.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.28 coercion site

- **Meaning:** A position where the compiler inserts an applicable implicit conversion automatically: a callable argument, including an argument of an intrinsic, a named field entry of a field-constructor call, or an entry of an enum-map declaration. It is *not* inserted where a value is written to a locally-fixed destination — a symbol declaration, an assignment or store, a `return`, or an `init{ }` — where the conversion is written explicitly, nor at an operator's operand, which is passed as written.
- **Why this name:** "Coercion" is the standard term for an implicit, compiler-inserted type conversion, as opposed to an explicit cast; a *coercion site* names a position where that conversion is permitted. A coercion is backed by an `implicit` constructor, including the literal constructors `core` supplies — the site says where one may be inserted, not that arbitrary conversion is built in.
- **Canonical home:** [`types.md`](types.md) §4.2

### 3.29 mould

- **Meaning:** One of the three constructs that give a type its shape: `struct`, `variant`, and `enum`. Each has a value form and a `#` reference form, and a mould appears only as the right-hand side of a `type` or `alias` declaration, so every constructible type is named.
- **Why this name:** A mould gives shapeless material a fixed form, which is what these three do to a type; the word also carries that a mould is the form a type is cast from.
- **Canonical home:** [`types.md`](types.md) §5.3

### 3.30 value mould / reference mould

- **Meaning:** A mould is written in one of two forms: a **value form**, unmarked, or a **reference form**, carrying a leading `#`. The form determines whether the declared type is copied and transitively value, or identity-bearing, moved, and accessible through references. It does not determine whether the type may recurse — both forms may, through a boxed member (§3.39).
- **Why this name:** The `#` mark names one axis — value versus reference — that crosses every mould.
- **Canonical home:** [`types.md`](types.md) §2.1

### 3.31 product mould / sum mould / peer mould

- **Meaning:** The three mould shapes, named by how a value's representations count. A `struct` is a **product mould** (its representations are the product of its fields'); a `variant` is a **sum mould** (the sum of its cases' payloads'); an `enum` is a **peer mould** — a flat set of payloadless, equal-rank peers, so its representations number exactly its members.
- **Why this name:** Product and sum are the standard algebraic names for the two `{ }`-bodied shapes; "peer" names the third from the `enum`'s own defining property — uniform, interchangeable, payloadless members — rather than forcing it into the sum family it degenerately belongs to.
- **Canonical home:** [`types.md`](types.md) §2.5 (product, sum); [`adt.md`](adt.md) §2 (peer)

### 3.32 owner

- **Meaning:** A source-facing symbol, field, or container slot that stores a reference-type object — or its owning handle — and governs that object's lifetime. Every reference-type object has exactly one owner at a time, settled (§3.23) or roaming (§3.24). Moving a roaming object transfers it to a new owner.
- **Why this name:** It is the ordinary owner of other languages — one per object, governing its lifetime, handed on by a move — so the plain word needs no metaphor.
- **Canonical home:** [`memory.md`](memory.md) §2.1

### 3.33 reference

- **Meaning:** The source-facing `&T`: access to a settled owner (§3.23) without storing that object or controlling its lifetime. A reference may be repointed, copied when assigned or passed, stored in an `&` field or element, or returned as `&T`, but it cannot outlive its owner. It is represented as its owner's 32-bit offset from the program's image.
- **Why this name:** Readers already know a reference as something that reaches an object it does not own and must not outlive it, which is the rule. A **reference type** is exactly the kind of type a reference may name.
- **Canonical home:** [`memory.md`](memory.md) §2.4; representation in [`memory.md`](memory.md) §4.1

### 3.34 taken parameter

- **Meaning:** A `^T` parameter with `T` a reference type, which takes a roaming owner or a temporary from the caller; filled with a value type, it is a borrow, and it is never filled with an `&` type. Passing a roaming owner symbol spends it (§3.44). The parameter is then a roaming owner of the body, which moves it on or lets it die when the body drains.
- **Why this name:** The callee *takes* the owner, plainly and for good, in contrast to a borrow it gives back and a reference it only names.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.8

### 3.35 relay / consume

- **Meaning:** The two ways a verb can treat an owner it takes (§3.34), told apart by its return. It **relays** the owner when it returns `^T`; the caller may bind that return to own the object again. It **consumes** the owner when it returns no owner. A verb that declares `T` or `&T` instead borrows or takes a reference, and leaves the caller's owner unchanged.
- **Why this name:** "Consume" names taking the value for good; "relay" names passing ownership through and handing it back out.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.8

### 3.36 reference source

- **Meaning:** A settled place a new `&` may be minted from: a bare settled symbol, a path from a settled root or an `&T` parameter through struct fields and `ArrayRef` elements only, or an `&T` parameter itself. `ListRef` elements and variant payloads are excluded. A roaming owner, and anything reached from one, never originates a reference.
- **Why this name:** The term names the *source* end — where a reference may come from — separately from where a stored reference may go, which is the store rule's business.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.37 passing mode

- **Meaning:** Which of three ways a reference-type argument reaches a callee, fixed entirely by the parameter's surface form: `T` **borrows** it (§3.27), `^T` **takes** it (§3.34), and `&T` takes a **reference** (§3.33). A value-type parameter is always a borrow, and the subject parameter (§3.38) always is one. An array literal's elements each travel in the mode written on its parameter's `@concepts$Array` element.
- **Why this name:** "Mode" names a choice of *how* the same argument is passed, not of *which type* it has: the type is unchanged in each mode, and only the caller's obligations and resulting state differ.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.38 subject / subject parameter / subject expression

- **Meaning:** The **subject** is the object a method is called on. The **subject parameter** is `this`, the declaration's first parameter, always written bare: it is a borrow (§3.27) for either kind of type, and no marker is written on it. The **subject expression** is what stands left of `:` or `!` at the call site and supplies the object.
- **Why this name:** Grammar, matching `verb` (§3.22): a call reads *subject–verb–object*, and the subject is what the verb acts from. The three senses are one word in ordinary use because they usually coincide; the spec separates them where a rule holds of the declaration but not the object, or the other way round.
- **Canonical home:** [`functions.md`](functions.md) §2.1

### 3.39 boxed member

- **Meaning:** A **member** — a `struct`/`#struct` field or a `variant`/`#variant` case payload alike — stored as a fixed-size handle inline, with the instance it names placed in the scope's dynamic region. Required for a variant payload on a cycle of **owning** edges that runs through no list's elements, and for a field on such a cycle that passes through no payload; permitted elsewhere for a payload. Nothing marks it in the source, and placement is unobservable (§3.25).
- **Why this name:** "Boxed" is the ordinary word for a value stored out of line behind a handle, and **member** rather than *field* because a `variant` case payload is boxed on the same terms as a `struct` field.
- **Canonical home:** [`adt.md`](adt.md) §4; representation in [`memory.md`](memory.md) §3.3 and §3.6

### 3.40 deep value copy

- **Meaning:** Copying an existing value copies the whole value — its inline bytes, plus a fresh allocation and recursive copy of the payload behind every boxed member (§3.39) it owns — so an original and its copy share no storage. Depth is what keeps a value transitively alias-free once it may own out-of-line storage, and so what lets a value type recurse (§3.2) and stay legal as a concurrent subject (§2.4).
- **Why this name:** "Deep" is the standard word for a copy that follows indirections instead of duplicating them, and the contrast it names — deep versus shallow — is precisely the choice the rule settles.
- **Canonical home:** [`memory.md`](memory.md) §2.3

### 3.41 move-source

- **Meaning:** An expression denoting a roaming value that the expression is entitled to consume, and therefore the only thing that may be moved into an owning position: a roaming symbol, a field of a roaming root, a `^T` result, or a `#variant` case form. A settled owner is never one.
- **Why this name:** It names the *source* end of a move, which is where the restriction lives: the rule is about what an expression is entitled to give up, not about where the value lands.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.2

### 3.42 carried reference

- **Meaning:** An `&` reachable from a value's **declared** type by following zero or more **owning** edges — members, payloads, and the elements of arrays and lists (§3.39). A type's own `&` field is the zero-edge case. The walk stops at each `&` rather than continuing through it into what it names, since that object is owned elsewhere and travels separately. A value's carried references are what the store rule compares alongside the value's owner.
- **Why this name:** The value *carries* the reference the way luggage carries its contents — the reference travels with it and is not part of what the value is used for, which is exactly why the store that relocates the value is the one that has to look inside.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.10

### 3.43 scope

- **Meaning:** The block whose lifetime bounds a place, and the only property of a place that the store rule compares. A **symbol**'s scope is its declaring block; a **field or element** takes its root symbol's scope rather than having one of its own; a `^T` parameter's is the body's top block; `this`, an `&T` parameter, and a constructor's `init{ }` have none in the body at all, each standing instead for a path in the caller's frame.
- **Why this name:** A place's scope is a lexical scope — a block — so the ordinary word fits. It names where a place's lifetime is bounded rather than where the place is written, which is the distinction the rule turns on — a field's own position does not bound its lifetime, and its root's scope does.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.1

### 3.44 spent symbol

- **Meaning:** A roaming owner symbol after its object has been moved out, whether by a direct move or by passing it to a taken parameter (§3.34). It denotes no object, so any use of it is a compile-time error, until a store **refills** it with a new owner. A symbol changes between owning and spent only in its declaration block, and a parameter, being read-only, is never refilled.
- **Why this name:** A spent casing has done its job and is empty, and it can be reloaded; the symbol has handed its object on and holds nothing, but keeps the storage for another.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.6

### 3.45 string primitive

- **Meaning:** `@primitives$String`, a value type with a fixed-size handle and owned bytes in the dynamic region. Copies own independent bytes; there is no terminator. `core` declares the value type `String` over it.
- **Why this name:** It is the compiler-provided storage for string contents, underneath package-defined string types.
- **Canonical home:** [`types.md`](types.md) §2.7

### 3.46 scalar primitive

- **Meaning:** `@primitives$I32`, `@primitives$I64`, `@primitives$F32`, or `@primitives$F64`: a signed integer or IEEE 754 float of the width its name gives. Each is built from its literal's concept, and converts to each other through a constructor of the target, named for its loss when it can lose a value.
- **Why this name:** A scalar is a single number, as against the containers and the string; the names spell out whether the number is an integer or a float and how many bits wide it is, which is all that separates the four.
- **Canonical home:** [`syntax.md`](syntax.md) §2.7; conversions in [`types.md`](types.md) §2.9

### 3.47 growing-argument rule

- **Meaning:** A type that leads back to itself, through any member or another type, passes back each argument as one of its own parameters or as one naming none. A parameter passed back inside a larger argument would demand a new type at every step, so the declaration is an error.
- **Why this name:** It names what the rule forbids: an argument that grows each time the type comes back to itself.
- **Canonical home:** [`generics.md`](generics.md) §2.4; its relation to boxing in [`adt.md`](adt.md) §4

### 3.48 byte primitive

- **Meaning:** `@primitives$U8`, an unsigned 8-bit integer from 0 to 255. It is built from `@concepts$Int`, converts to the integer scalars exactly and from them by `wrap`, and has `equal` and `lessThan` but no arithmetic. `core` declares the storage type `U8` over it.
- **Why this name:** A byte is the unit of storage it occupies, and *primitive* places it beside the scalar and string primitives; it is not a scalar primitive, since it has no arithmetic of its own.
- **Canonical home:** [`syntax.md`](syntax.md) §2.7; conversions in [`types.md`](types.md) §2.9

### 3.49 byte set

- **Meaning:** `@primitives$ByteSet`, a value type holding a set of the 256 byte values. `byteSet` builds one from 256 flags, and `findAny` and `findPair` search a string primitive's bytes for its members. Its layout is the compiler's, so a set comes only from `byteSet`.
- **Why this name:** It is a set whose members are bytes, and nothing more: the name leaves out the lookup table the compiler lays it out as.
- **Canonical home:** [`types.md`](types.md) §2.12

### 3.50 veiled value

- **Meaning:** A value whose type is a type parameter, inside the generic body that declares the parameter: `x` in `Float read(x T Type)`, a `this S Type` subject, or a `T` read from a field of `Box<T>`. The body may hold it and reach it through methods, functions, operators, subscripts and constructors; a field read, an enum map read, naming a member or case, a `match`, or a call as a function value on it is an error.
- **Why this name:** A veiled thing is present and whole but its face is hidden; the generic body handles the value without seeing its type, and the caller, who supplies the type, always sees it. The name names the restriction itself, and it borrows no meaning from "opaque", which the spec already uses for a foreign handle's C object.
- **Canonical home:** [`generics.md`](generics.md) §3.8

---

## 4. Packages, Operators, and Versioning

### 4.1 home-package operator rule

- **Meaning:** A source operator implementation may be declared only in the home package of one of its user-defined operand types. Operators over the fundamental types alone live in `core`, and a storage primitive, whose home is no package, has none.
- **Why this name:** The rule ties operator declarations to the package that "owns" one operand type and prevents unrelated helper imports from changing operator meaning.
- **Canonical home:** [`operators.md`](operators.md) §2.2

### 4.2 placeholder-prefix rewriting

- **Meaning:** During fetch, a library's `!`-prefixed export symbols are rewritten with the resolved version tag and the package's identity hash (§4.13) before caching and linking. Only the prefix changes; the name the symbol carries is the library package's own path within its project.
- **Why this name:** The shipped `!` prefix is only a placeholder marker; the toolchain rewrites that prefix into the real versioned symbol prefix.
- **Canonical home:** [`dependencies.md`](dependencies.md) §6.1

### 4.3 URL identity

- **Meaning:** A dependency's canonical identity is its project's full source URL, while manifest keys are only local labels.
- **Why this name:** The rule makes the repository URL itself the package's identity, rather than any alias that a project chooses locally.
- **Canonical home:** [`dependencies.md`](dependencies.md) §1 and §2

### 4.4 import spelling rule

- **Meaning:** An import states how its members are written in that file — qualified, aliased, or bare — and that is the only spelling available there.
- **Why this name:** The rule is about surface spelling rather than availability: every form reaches the same declaration, and the import chooses how it is written.
- **Canonical home:** [`packages.md`](packages.md) §3.3 and §3.4

### 4.5 intersection and union

- **Meaning:** The meaning of the operators `and` and `or`: what two values share and what either covers. On `Bool` they are conjunction and disjunction; on a set, its intersection and union. Both bind below every comparison.
- **Why this name:** The set-theoretic names hold for every type that implements the pair, where *conjunction* and *disjunction* fit `Bool` alone; the words `and` and `or` read as the same thing in prose.
- **Canonical home:** [`operators.md`](operators.md) §2.4

### 4.6 block argument

- **Meaning:** A braced run of statements passed to a call and executed by the callee. It captures its surroundings, never becomes a value, and cannot outlive the call.
- **Why this name:** It is an argument like any other, and what it carries is a block rather than a value.
- **Canonical home:** [`control-flow.md`](control-flow.md) §2

### 4.7 trailing argument

- **Meaning:** A call's last argument written after the closing `)` instead of inside it, with the `)` elided. Only a `{ }` argument may trail — a block or a map literal — at most one per call, and it must be the last item in its statement.
- **Why this name:** It trails the argument list rather than sitting in it.
- **Canonical home:** [`syntax.md`](syntax.md) §4.8

### 4.8 intrinsic

- **Meaning:** Anything reached through `@` — a type, operation, or instance supplied by the compiler rather than declared by a package. Each `@` namespace is an intrinsic namespace holding one kind of intrinsic.
- **Why this name:** The everyday word already means "built into the thing itself", which is what sets these apart from every package declaration; since it covers the whole `@` space, no single namespace is named for it.
- **Canonical home:** [`syntax.md`](syntax.md) §2.7

### 4.9 control-flow intrinsic

- **Meaning:** `@controlflow$branch`, `@controlflow$repeat`, `@controlflow$exitFromCall`, `@controlflow$work`, and `@controlflow$register`, the five intrinsics that build every branching, repeating, and exiting construct and the single worker; a worker pool is a primitive of its own. Each is stated over storage primitives, function values, a subject of any type, or nothing, and callable from any package.
- **Why this name:** They are the intrinsics (§4.8) that are operations of control flow.
- **Canonical home:** [`control-flow.md`](control-flow.md) §4.1

### 4.10 ordinary `core`

- **Meaning:** `core` declares the fundamental types but holds no standing in the language: it is fetched, versioned, pinned, imported, and remapped like any other dependency, and two of its versions may coexist in one program.
- **Why this name:** The label records the whole rule — what is notable about `core` is precisely that nothing about it is special.
- **Canonical home:** [`types.md`](types.md) §2.6 and [`dependencies.md`](dependencies.md) §14

### 4.11 spawn target

- **Meaning:** Only a function or method call may be spawned, and never one whose verb declares a block parameter, since a block captures the frame that wrote it.
- **Why this name:** The term names the position the restriction applies to — what a `spawn` may point at.
- **Canonical home:** [`concurrency.md`](concurrency.md) §3.1

### 4.12 root package

- **Meaning:** The package a build starts from — the program package being built, the plugin package of a dynamic-library build, or the test package of a test build. Apart from exported lambdas, only it reaches `@program$` and declares foreign variables; a program or test package holds `main`; a library package is never one.
- **Why this name:** Every other package in the build is reached from it through the dependency graph, whose root it is; whether a package is the root depends on the build, not on the package.
- **Canonical home:** [`packages.md`](packages.md) §6.1

### 4.13 identity hash

- **Meaning:** A fixed-length hash of a package's normalized URL, written into every rewritten symbol after the version tag. It keeps two packages that share a name distinct at link time.
- **Why this name:** It is a hash of the package's identity — its URL — and stands in for that identity inside a symbol name.
- **Canonical home:** [`dependencies.md`](dependencies.md) §6.1

### 4.14 test package

- **Meaning:** A directory of a project's `test/` holding `.zn` files, each declaring `package test`. It imports what the tested package's real user imports and is the root package of its own test build.
- **Why this name:** It is the package that tests the others, and its name is fixed, so every test package is declared with the name `test`.
- **Canonical home:** [`packages.md`](packages.md) §7

### 4.15 library package / program package

- **Meaning:** A **library package** is a directory of a project's `lib/`, importable by the project's other packages and, unless its name begins with `_`, by other projects. A **program package** is a directory of `bin/`, holding `main`, which nothing imports. A directory of `bin/` without `main` is a **plugin package** (§4.25).
- **Why this name:** The directory names already show it: `lib/` holds the packages that other packages link against, and `bin/` holds the packages that become programs.
- **Canonical home:** [`packages.md`](packages.md) §2.1

### 4.16 subpackage

- **Meaning:** A directory with `.zn` files inside a library package, nested to any depth. Only its parent imports it, and no other project sees it.
- **Why this name:** It is a package in every respect, placed under another one that alone reaches it.
- **Canonical home:** [`packages.md`](packages.md) §2.3

### 4.17 machine operation

- **Meaning:** A function of `@operators$`: the machine arithmetic, comparison, Boolean connective, or string join on storage primitives that a package writes its operators over. Each takes operands of one type, with one overload per type it takes.
- **Why this name:** It is the operation the machine performs, which an operator presents in notation: it is called by name like any function, where an operator is written between its operands.
- **Canonical home:** [`operators.md`](operators.md) §2.6

### 4.18 math function / exact / approximate

- **Meaning:** A function of `@math$`, on scalar primitives. An **exact** one's result is fully specified and the same on every platform; an **approximate** one's is close to the true value and may differ between platforms.
- **Why this name:** The everyday words: the namespace holds what a math library holds, and the words exact and approximate state whether a function's result can be trusted to the last bit.
- **Canonical home:** [`math.md`](math.md) §2.2

### 4.19 foreign verb

- **Meaning:** A function or method whose body is a C function, named by its symbol in place of a body. Its Zane signature is checked against the C declaration, and the signature states everything the verb touches outside the program.
- **Why this name:** "Foreign" is the established word for code in another language reached through an interface, as in a foreign-function interface; "verb" is the spec's word for a function or method.
- **Canonical home:** [`ffi.md`](ffi.md) §3.1

### 4.20 foreign handle / data handle / outside handle / foreign capability

- **Meaning:** A **foreign handle** (`#handle`) is a reference type referring to one C object. One without a free function is a **foreign capability**, referring to a C global; one with a free function owns its object, and is an **outside handle**, part of capability-backed state, when a method on a foreign capability makes it, and a **data handle**, state of the program's own, otherwise.
- **Why this name:** C APIs already call an opaque pointer to their object a handle; "capability" is the effect model's term for an object that reaches outside state.
- **Canonical home:** [`ffi.md`](ffi.md) §4.1

### 4.21 interface header

- **Meaning:** A `.h` file directly in a package's directory. Every C function a foreign verb names is declared in one, and every C type a foreign handle names is declared in one or in a header it includes; the rest of any other header is read by the C compiler alone.
- **Why this name:** It is the header that states the interface between the package's C and its Zane, as opposed to the C library's own headers.
- **Canonical home:** [`ffi.md`](ffi.md) §2.1

### 4.22 foreign variable

- **Meaning:** A symbol declared with `foreign` and a C global's symbol in place of an initializer, whose value is a foreign capability referring to that global. Only the root package and exported lambdas declare one, and pass it on.
- **Why this name:** It is the variable counterpart of a foreign verb: declared like any symbol, with its value supplied by C.
- **Canonical home:** [`ffi.md`](ffi.md) §5.1

### 4.23 package rename / dropped package

- **Meaning:** A `renames` row of a project's manifest gives a dependency's public library package another name in that project, the only name its files import it by. A **dropped package**, renamed to `_`, is importable by none of them and claims no name. Neither changes a compiled symbol.
- **Why this name:** It renames the package for a whole project, where an `as` alias respells it in one file; `_` is the name that already means hidden.
- **Canonical home:** [`dependencies.md`](dependencies.md) §2.1 and §8

### 4.24 `comptime` call

- **Meaning:** A function or `:` method call marked `comptime`, evaluated at compile time or the build fails. Unlike ordinary compile-time evaluation it may read capability-backed state, through capabilities that stand for the build machine's own; it writes no capability-backed state, and its result is a value type built into the program.
- **Why this name:** The keyword marks the stage in which the call runs. It is Zig's word for compile-time evaluation, so a reader who knows Zig understands it correctly, and it names a stage rather than a property of the verb.
- **Canonical home:** [`effects.md`](effects.md) §5.6

### 4.25 plugin package / dynamic library

- **Meaning:** A **plugin package** is a directory of `bin/` that declares no `main` and carries `export` lines or exported lambdas. The toolchain builds it as a **dynamic library** (`.so`, `.dylib`, `.dll`) that another program loads into its own process and calls through the exported lambdas.
- **Why this name:** "Plugin" is the common word for code that a program loads to extend itself, whatever the program is: an audio workstation, a Python interpreter, a game engine. "Dynamic library" is the platforms' own name for the file it builds.
- **Canonical home:** [`packages.md`](packages.md) §8.1

### 4.26 exported lambda / export line

- **Meaning:** An **exported lambda** is a package-scope lambda followed by `export "cName"`; in a build that exports it, it becomes a C function of that name, with the rights of root-package code. An **export line**, `export lib;`, sits in a plugin or test package and names the library whose exported lambdas this build turns into C symbols.
- **Why this name:** "Export" is the word C toolchains already use for a symbol a dynamic library offers, and it is the mirror of `foreign`, which brings a C symbol in.
- **Canonical home:** [`packages.md`](packages.md) §8.2, §8.3

### 4.27 C-held object

- **Meaning:** An object an exported lambda returned to C as `^T`. It is settled in a top-level scope of its own, C keeps it only as an opaque pointer, exported lambdas borrow it, and an exported lambda that takes it as `^T` ends it. No `&` connects two C-held objects.
- **Why this name:** It names who holds the owner. The object is an ordinary Zane object; only the holder differs.
- **Canonical home:** [`packages.md`](packages.md) §8.5

### 4.28 context

- **Meaning:** The subject of a foreign method whose type is not a handle. C receives it as an opaque `void *` and passes it back, during the call, as the first argument of the call's function values that declare the same subject; they may write it only when the foreign method is `mut`.
- **Why this name:** C APIs call the pointer a callback receives back its context or user data; the Zane term keeps the first word.
- **Canonical home:** [`ffi.md`](ffi.md) §3.8

### 4.29 C-compatible element type

- **Meaning:** A scalar primitive, `@primitives$U8`, or a C-compatible value type. An array or a list of one crosses into C as a view, so only the element type decides whether a container crosses.
- **Why this name:** It names the element, because the element, not the container, decides whether C can read the memory.
- **Canonical home:** [`ffi.md`](ffi.md) §3.3

### 4.30 host / host rule

- **Meaning:** A **host** is the program that loads a dynamic library into its own process and calls its exported lambdas, such as an audio workstation loading a CLAP plugin. The **host rule** is what every host obeys: it never runs two calls that borrow one C-held object mutably at once, and never runs a call that takes a C-held object's owner beside any other call on it.
- **Why this name:** "Host" is the word every plugin format already uses for the loading program. The spec once used it for an owner in the memory model; that meaning is retired, and the word now has only this one.
- **Canonical home:** [`packages.md`](packages.md) §8.1; host rule [`concurrency.md`](concurrency.md) §4.7
