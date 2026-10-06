# Zane Terminology

This document records the canonical names used across the Zane specification for recurring language concepts and design rules. It does not add semantics; each term points back to the document that defines the rule in full and explains why that label is the preferred name.

> **See also:** [`README.md`](../README.md) for the document index. [`syntax.md`](syntax.md) for canonical surface forms. Topic documents for the full semantic rules behind each term.

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

- **Meaning:** A spawned call may mutate only a value-typed subject, and at most one live spawn may mutably borrow a given storage location. A value type is transitively alias-free, so the rule rules out an aliased data race from the subject's type alone; concurrent reads take a coherent snapshot instead of serializing.
- **Why this name:** Concurrent mutation is gated on the subject being a value type — the property that makes race-freedom checkable without whole-program alias analysis.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.2 and §4.3

### 2.5 water-tower lifetimes

- **Meaning:** Objects owned in a scope stay alive until every `spawn`ed call in that scope has completed and the scope drains.
- **Why this name:** The source document explains the rule through a water-tower analogy in which each still-running spawned call acts like a plate holding the water level up.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.1

### 2.6 structural effect model

- **Meaning:** What a verb may write follows from its kind — a `mut` method writes `this`, a method without `mut` or a function writes nothing its caller can see — and capability access follows from call structure, with no effect annotations.
- **Why this name:** The model is "structural" because effects follow from program structure and reachable state, not from separate effect declarations.
- **Canonical home:** [`effects.md`](effects.md) §1, §3, and §5

### 2.7 capability wiring

- **Meaning:** Capability objects must be passed or stored explicitly so access to external state remains visible in the object graph and call graph. They originate in `@program$`, which only the root package reaches.
- **Why this name:** The design treats capabilities like explicit wiring between components rather than ambient globals.
- **Canonical home:** [`effects.md`](effects.md) §6

### 2.8 1-based ordinal counting

- **Meaning:** Counted repetition and positional indexing start at `1`, so an ordered sequence's final valid position is its size.
- **Why this name:** The term makes the rule about ordinal positions explicit and distinguishes it from raw numeric arithmetic.
- **Canonical home:** [`control-flow.md`](control-flow.md) §5

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
- **Why this name:** These are compiler-defined concept-level placeholders for source literals, not ordinary user storage types. Each literal concept is named for what that literal is usually called across languages, so `@concepts$Int` sits beside `@primitives$Int`, the storage primitive whose constructor takes it.
- **Canonical home:** [`syntax.md`](syntax.md) §2.8–§2.12; lowering in [`types.md`](types.md) §2.7

### 3.5 field constructor

- **Meaning:** Constructor syntax may declare fields directly in the parameter header and map them into `init{}`.
- **Why this name:** The written constructor header is shaped around fields themselves rather than around separate parameter names.
- **Canonical home:** [`types.md`](types.md) §3.3

### 3.6 method-based privacy

- **Meaning:** `_` fields are private to methods whose first parameter is `this` for that type, rather than to a package boundary.
- **Why this name:** Privacy is granted by the method/subject relationship, not by where the function is declared.
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

- **Meaning:** `type Name = T` introduces a new distinct type that is structurally equal to `T` but not interchangeable with it; `alias Name = T` introduces a fully interchangeable name. The keyword carries the distinction.
- **Why this name:** The pairing names the only difference between the two declaration forms — whether the result is a new type or just another name.
- **Canonical home:** [`types.md`](types.md) §5

### 3.13 casing-determined kind

- **Meaning:** The first letter of an identifier selects its lexical class: an uppercase-initial name is a type, a lowercase-initial name is a value, binding, or parameter. A lowercase name in a type position is a compile-time error.
- **Why this name:** Casing alone, not a declaration or lookahead, determines whether a bare name is a type or a value.
- **Canonical home:** [`lexical.md`](lexical.md) §3

### 3.14 `Type` and `@concepts$Int` parameter concepts

- **Meaning:** `Type` and `@concepts$Int` are compiler concept types used to declare type and number parameters (`T Type`, `n @concepts$Int`) — the only two kinds a type's `<>` header holds. Like other concept types they are legal only in parameter positions, never as storage. As `()` value parameters they are passed explicitly; introduced inline on a verb parameter's type or nested type they are inferred; listed in a type's `<>` header they are applied positionally at use sites.
- **Why this name:** A type or size handed to a declaration is a compile-time value, so its parameter has a concept type like any other — for a size, the one an integer literal already carries — rather than a bespoke parameter-kind keyword.
- **Canonical home:** [`generics.md`](generics.md) §3.3

### 3.15 variant (sum mould)

- **Meaning:** A `variant` is a **sum mould**: a value of the type it declares holds exactly one of its named members at a time. Its body grammar is identical to a `struct` — the product mould — with the keyword flipping product into sum. Reading a member is partial and therefore abortable.
- **Why this name:** "Variant" is the established name for a tagged sum of alternatives, and it reads as a peer of `struct` since the two share one body grammar.
- **Canonical home:** [`adt.md`](adt.md) §3

### 3.16 enum (uniform peers)

- **Meaning:** An `enum` is a closed set of interchangeable, payloadless peer members that mean one uniform thing (colors, weekdays). It is not a sum mould; per-member data is attached externally by an enum map.
- **Why this name:** "Enum" matches the common meaning of an enumeration of equal-rank constants, and the spec reserves it for that uniform-peer role rather than overloading it with the sum-type role given to `variant`.
- **Canonical home:** [`adt.md`](adt.md) §2

### 3.17 struct/variant body symmetry

- **Meaning:** A `struct` body and a `variant` body use the exact same grammar; the keyword alone decides product versus sum. The symmetry applies to the declaration, not to consuming code, where construction and reads differ.
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

- **Meaning:** The bracket decides how the things inside it are separated: a `{ }` terminates each with `;` — entries of a body, always trailing, and statements of a code block alike, save a statement a `}` already ends; a `[ ]`, `( )`, or `< >` list separates its entries with `,`, never trailing.
- **Why this name:** The rule turns on the bracket alone rather than on what the construct means, so the name states what does the picking.
- **Canonical home:** [`lexical.md`](lexical.md) §6

### 3.22 verb

- **Meaning:** A callable whose body is a sequence of statements that executes to do work. The verbs are functions, methods, operators, constructors, and lambdas (a lambda being an anonymous verb, the only verb that also has a value form). A subscript is **not** a verb: its body must be a place expression that projects existing storage rather than running computation, so it designates a place instead of executing.
- **Why this name:** The unifying trait is the executing statement body — a verb *does* something — which is why a constructor (statements ending in `return init{}`) counts and is indistinguishable from a builder helper apart from its `init{}` sugar, while a place-projecting subscript does not.
- **Canonical home:** [`functions.md`](functions.md) §1

### 3.23 settled owner

- **Meaning:** An owner that may be referenced and never moves again: a bare reference-type symbol, or a field or `ArrayRef` element of a settled root. A roaming owner settles by moving into a settled place. Overwriting it writes the replacement at the same address.
- **Why this name:** A settler has stopped travelling and taken a fixed home; a reference can name only an owner that has done so, and the word says the owner has stopped moving for good.
- **Canonical home:** [`memory.md`](memory.md) §2.1

### 3.24 roaming owner

- **Meaning:** An owner that may move anywhere. Neither it nor anything inside it can be referenced. It is a symbol, parameter, return, or abort written `^T` with `T` a reference type, a field or `ArrayRef` element of a roaming root, or any list element or variant payload.
- **Why this name:** The opposite of *settled* in the same register: an owner still travelling, which no reference can name. *Loose* was set aside because the spec already calls `'*` the loose form of an operator.
- **Canonical home:** [`memory.md`](memory.md) §2.1

### 3.25 arena placement

- **Meaning:** Where an owned object's storage is materialized: the arena of the scope that creates it, split into a **fixed-size** region for statically sized slots and the handles that sit in them, and a **dynamic** region for the payloads those handles name. Placement is an unobservable implementation choice.
- **Why this name:** Placement is a choice among **arenas** — the per-scope regions — rather than between a stack and a heap; the creating scope's arena is the default, a parent arena the fallback on escape.
- **Canonical home:** [`memory.md`](memory.md) §3.5

### 3.26 capability marker

- **Meaning:** A surface marker on a verb that selects its kind and unlocks one capability: naming the first parameter `this` makes a method and grants private-field access; naming the verb after a type makes a constructor, implying its return type and unlocking `init{ }`; a symbol name makes an operator; no name makes a lambda. The parameter system, body grammar, overload resolution, and effect model are shared across all verbs.
- **Why this name:** The marker is a small piece of surface form that, by its presence, grants a *capability* to an otherwise-ordinary verb — so a constructor is a verb with one marker, not a separate mechanism.
- **Canonical home:** [`functions.md`](functions.md) §8

### 3.27 borrow

- **Meaning:** Non-owning, non-escaping access to a caller's storage for the duration of a call: a bare parameter of either kind of type, and every subject. It is the only way a value type is passed. A borrow is mutable only as a `mut` subject, nothing else in the call writes the place it lends, and a value is copied only when bound into a fresh slot.
- **Why this name:** The callee is lent the caller's storage for the call and gives it back at return — it does not own it and cannot keep it. Unlike a reference, the borrow itself cannot be stored, returned, or used as a move-source — a restriction on the borrow, not on the value read through it, which a value type may still copy into a fresh slot.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.28 coercion site

- **Meaning:** A position where the compiler inserts an applicable implicit conversion automatically: a callable argument, including an argument of an intrinsic, a named field entry of a field-constructor call, or an entry of an enum-map declaration. It is *not* inserted where a value is written to a locally-fixed destination — a symbol declaration, an assignment or store, a `return`, or an `init{ }` — where the conversion is written explicitly.
- **Why this name:** "Coercion" is the standard term for an implicit, compiler-inserted type conversion, as opposed to an explicit cast; a *coercion site* names a position where that conversion is permitted. A coercion is backed by an `implicit` constructor, including the literal constructors `core` supplies — the site says where one may be inserted, not that arbitrary conversion is built in.
- **Canonical home:** [`types.md`](types.md) §4.2

### 3.29 mould

- **Meaning:** One of the three constructs that give a type its shape: `struct`, `variant`, and `enum`. Each has a value form and a `#` reference form, and a mould appears only as the right-hand side of a `type` or `alias` declaration, so every constructible type is named.
- **Why this name:** A mould gives shapeless material a fixed form, which is what these three do to a type; the word also carries that a mould is the form a type is cast from.
- **Canonical home:** [`types.md`](types.md) §5.3

### 3.30 value mould / reference mould

- **Meaning:** A mould is written in one of two forms: a **value form**, unmarked, or a **reference form**, carrying a leading `#`. The form decides whether the declared type is copied and transitively value, or identity-bearing, moved, and accessible through references. It does not decide whether the type may recurse — both forms may, through a boxed member (§3.39).
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

- **Meaning:** The source-facing `&T`: access to a settled owner (§3.23) without storing that object or controlling its lifetime. A reference may be repointed, copied when assigned or passed, stored in an `&` field or element, or returned as `&T`, but it cannot outlive its owner. It is represented by the owner's segmented offset.
- **Why this name:** Readers already know a reference as something that reaches an object it does not own and must not outlive it, which is the rule. A **reference type** is exactly the kind of type a reference may name.
- **Canonical home:** [`memory.md`](memory.md) §2.4

### 3.34 taken parameter

- **Meaning:** A `^T` parameter with `T` a reference type, which takes a roaming owner or a temporary from the caller; filled with a value type, it is a borrow, and it is never filled with an `&` type. Passing a roaming owner symbol spends it (§3.44). The parameter is then a roaming owner of the body, which moves it on or lets it die when the body drains.
- **Why this name:** The callee *takes* the owner, plainly and for good, in contrast to a borrow it gives back and a reference it only names.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.8

### 3.35 relay / consume

- **Meaning:** The two ways a verb can treat an owner it takes (§3.34), told apart by its return. It **relays** the owner when it returns `^T`; the caller may bind that return to own the object again. It **consumes** the owner when it returns no owner. A verb that declares `T` or `&T` instead borrows or takes a reference, and leaves the caller's owner unchanged.
- **Why this name:** "Consume" names taking the value for good; "relay" names passing ownership through and handing it back out.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.8

### 3.36 reference source

- **Meaning:** A settled place a new `&` may be minted from: a bare settled symbol, a path from a settled root or an `&T` parameter through struct fields and `ArrayRef` elements only, or an `&T` parameter itself. List elements and variant payloads are excluded. A roaming owner, and anything reached from one, never originates a reference.
- **Why this name:** The term names the *source* end — where a reference may come from — separately from where a stored reference may go, which is the store rule's business.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.37 passing mode

- **Meaning:** Which of three ways a reference-type argument reaches a callee, fixed entirely by the parameter's surface form: `T` **borrows** it (§3.27), `^T` **takes** it (§3.34), and `&T` takes a **reference** (§3.33). A value-type parameter is always a borrow, and the subject parameter (§3.38) always is one.
- **Why this name:** "Mode" names a choice about *how* the same argument travels rather than *what* it is — the type is unchanged in each, and only the caller's obligations and resulting state differ.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.38 subject / subject parameter / subject expression

- **Meaning:** The **subject** is the object a method is called on. The **subject parameter** is `this`, the declaration's first parameter, always written bare: it is a borrow (§3.27) for either kind of type, and no marker is written on it. The **subject expression** is what stands left of `:` or `!` at the call site and supplies the object.
- **Why this name:** Grammar, matching `verb` (§3.22): a call reads *subject–verb–object*, and the subject is what the verb acts from. The three senses are one word in ordinary use because they usually coincide; the spec separates them where a rule holds of the declaration but not the object, or the other way round.
- **Canonical home:** [`functions.md`](functions.md) §2.1

### 3.39 boxed member

- **Meaning:** A **member** — a `struct`/`#struct` field or a `variant`/`#variant` case payload alike — stored as a fixed-size handle inline, with the instance it names placed in the scope's dynamic region. Required where a member's declared type can lead back to the enclosing type along **owning** edges, and permitted elsewhere. Nothing marks it in the source, and placement is unobservable (§3.25).
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

- **Meaning:** An `&` reachable from a value's **declared** type by following zero or more **owning** edges — the same graph the boxed-member rule reads (§3.39). A type's own `&` field is the zero-edge case. The walk stops at each `&` rather than continuing through it into what it names, since that object is owned elsewhere and travels separately. A value's carried references are what the store rule compares alongside the value's owner.
- **Why this name:** The value *carries* the reference the way luggage carries its contents — the reference travels with it and is not part of what the value is used for, which is exactly why the store that relocates the value is the one that has to look inside.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.10

### 3.43 scope

- **Meaning:** The block whose lifetime bounds a place, and the only thing the store rule compares. A **symbol**'s scope is its declaring block; a **field or element** takes its root symbol's scope rather than having one of its own; a `^T` parameter's is the body's top block; `this`, an `&T` parameter, and a constructor's `init{ }` have none in the body at all, each standing instead for a path in the caller's frame.
- **Why this name:** A place's scope is a lexical scope — a block — so the ordinary word fits. It names where a place's lifetime is bounded rather than where the place is written, which is the distinction the rule turns on — a field's own position tells you nothing, its root's scope tells you everything.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.1

### 3.44 spent symbol

- **Meaning:** A roaming owner symbol after its object has been moved out, whether by a direct move or by passing it to a taken parameter (§3.34). It denotes no object, so any use of it is a compile-time error, until a store **refills** it with a new owner. A symbol changes between owning and spent only in its declaration block, and a parameter, being read-only, is never refilled.
- **Why this name:** A spent casing has done its job and is empty, and it can be reloaded; the symbol has handed its object on and holds nothing, but keeps the storage for another.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.6

### 3.45 string primitive

- **Meaning:** `@primitives$String`, a value type with a fixed-size handle and owned bytes in the dynamic region. Copies own independent bytes; there is no terminator. `core` declares the value type `String` over it.
- **Why this name:** It is the compiler-provided storage for string contents, underneath package-defined string types.
- **Canonical home:** [`types.md`](types.md) §2.7

---

## 4. Packages, Operators, and Versioning

### 4.1 home-package operator rule

- **Meaning:** A source operator implementation may be declared only in the home package of one of its user-defined operand types. Operators over the fundamental types alone live in `core`.
- **Why this name:** The rule ties operator declarations to the package that "owns" one operand type and prevents unrelated helper imports from changing operator meaning.
- **Canonical home:** [`operators.md`](operators.md) §2.2

### 4.2 placeholder-prefix rewriting

- **Meaning:** During fetch, a library's `!`-prefixed export symbols are rewritten with the resolved version tag and the package's identity hash (§4.13) before caching and linking. Only the prefix changes; the name the symbol carries is the library package's own path within its project.
- **Why this name:** The shipped `!` prefix is only a placeholder marker; the toolchain rewrites that prefix into the real versioned symbol prefix.
- **Canonical home:** [`dependencies.md`](dependencies.md) §6.1

### 4.3 URL identity

- **Meaning:** A dependency's canonical identity is its project's full source URL, while manifest keys are only local labels.
- **Why this name:** The rule says identity comes from the repository URL itself, not from whichever alias a project chooses locally.
- **Canonical home:** [`dependencies.md`](dependencies.md) §1 and §2

### 4.4 import spelling rule

- **Meaning:** An import states how its members are written in that file — qualified, aliased, or bare — and that is the only spelling available there.
- **Why this name:** The rule is about surface spelling rather than availability: every form reaches the same declaration, and the import chooses how it is written.
- **Canonical home:** [`packages.md`](packages.md) §3.3 and §3.4

### 4.5 loose form

- **Meaning:** A binary operator written with a leading `'`, calling the same implementation at a mirrored precedence level below every unprefixed operator. One tier only, binary only.
- **Why this name:** The prefix loosens how tightly the operator binds and changes nothing else about it.
- **Canonical home:** [`operators.md`](operators.md) §3.1

### 4.6 block argument

- **Meaning:** A braced run of statements passed to a call and executed by the callee. It captures its surroundings, never becomes a value, and cannot outlive the call.
- **Why this name:** It is an argument like any other, and what it carries is a block rather than a value.
- **Canonical home:** [`control-flow.md`](control-flow.md) §2

### 4.7 trailing argument

- **Meaning:** A call's last argument written after the closing `)` instead of inside it, with the `)` elided. Only a `{ }` argument may trail — a block or a map literal — at most one per call, and it must be the last thing in its statement.
- **Why this name:** It trails the argument list rather than sitting in it.
- **Canonical home:** [`syntax.md`](syntax.md) §4.8

### 4.8 intrinsic

- **Meaning:** Anything reached through `@` — a type, operation, or instance supplied by the compiler rather than declared by a package. Each `@` namespace is an intrinsic namespace holding one kind of intrinsic.
- **Why this name:** The everyday word already means "built into the thing itself", which is what sets these apart from every package declaration; since it covers the whole `@` space, no single namespace is named for it.
- **Canonical home:** [`syntax.md`](syntax.md) §2.7

### 4.9 control-flow intrinsic

- **Meaning:** `@controlflow$branch`, `@controlflow$repeat`, and `@controlflow$exitFromCall`, the three intrinsics every branching, repeating, and exiting construct is built from. Each is stated over storage primitives or over nothing, and callable from any package.
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

- **Meaning:** The package a program's build starts from — the program package being built, or the test package of a test build. Only it reaches `@program$`, and it holds `main`; a library package is never one.
- **Why this name:** Every other package in the build is reached from it through the dependency graph, whose root it is; whether a package is the root depends on the build, not on the package.
- **Canonical home:** [`packages.md`](packages.md) §6.1

### 4.13 identity hash

- **Meaning:** A fixed-length hash of a package's normalized URL, written into every rewritten symbol after the version tag. It keeps two packages that share a name distinct at link time.
- **Why this name:** It is a hash of the package's identity — its URL — and stands in for that identity inside a symbol name.
- **Canonical home:** [`dependencies.md`](dependencies.md) §6.1

### 4.14 test package

- **Meaning:** A directory of a project's `test/` holding `.zn` files, each declaring `package test`. It imports what the tested package's real user imports and is the root package of its own test build.
- **Why this name:** It is the package that tests the others, and its name is fixed, so every test package's declaration says `test`.
- **Canonical home:** [`packages.md`](packages.md) §7

### 4.15 library package / program package

- **Meaning:** A **library package** is a directory of a project's `lib/`, importable by the project's other packages and, unless its name begins with `_`, by other projects. A **program package** is a directory of `bin/`, holding `main`, which nothing imports.
- **Why this name:** The directories already say it: `lib/` holds what others link against, `bin/` what becomes a program.
- **Canonical home:** [`packages.md`](packages.md) §2.1

### 4.16 subpackage

- **Meaning:** A directory with `.zn` files inside a library package, nested to any depth. Only its parent imports it, and no other project sees it.
- **Why this name:** It is a package in every respect, placed under another one that alone reaches it.
- **Canonical home:** [`packages.md`](packages.md) §2.3
