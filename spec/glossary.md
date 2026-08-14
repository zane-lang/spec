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

### 2.3 scope-exit guard
- **Meaning:** `guard` conditionally exits the current lexical scope instead of introducing another nested branch.
- **Why this name:** The term emphasizes that `guard` is about leaving the surrounding scope, not about starting a new control-flow block.
- **Canonical home:** [`control-flow.md`](control-flow.md) §3

### 2.4 value-typed mutation rule
- **Meaning:** A spawned call may mutate only a value-typed subject, and at most one live spawn may mutably borrow a given storage location. A value type is transitively alias-free, so the rule rules out an aliased data race from the subject's type alone; concurrent reads take a coherent snapshot instead of serializing.
- **Why this name:** Concurrent mutation is gated on the subject being a value type — the property that makes race-freedom checkable without whole-program alias analysis.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.2 and §4.3

### 2.5 water-tower lifetimes
- **Meaning:** Scope-hosted objects stay alive until every `spawn`ed call in that scope has completed and the scope drains.
- **Why this name:** The source document explains the rule through a water-tower analogy in which each still-running spawned call acts like a plate holding the water level up.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.1

### 2.6 structural effect model
- **Meaning:** Effect level is inferred from `mut`, hosting, call structure, and reachable capabilities rather than from a large set of written annotations.
- **Why this name:** The model is "structural" because the compiler derives effects from program structure and reachable state, not from separate effect declarations.
- **Canonical home:** [`effects.md`](effects.md) §1 and §5

### 2.7 capability wiring
- **Meaning:** Capability objects must be passed or stored explicitly so access to external state remains visible in the object graph and call graph.
- **Why this name:** The design treats capabilities like explicit wiring between components rather than ambient globals.
- **Canonical home:** [`effects.md`](effects.md) §6

### 2.8 1-based ordinal counting
- **Meaning:** Counted loops and positional indexing start at `1`, so an ordered sequence's final valid position is its size.
- **Why this name:** The term makes the rule about ordinal positions explicit and distinguishes it from raw numeric arithmetic.
- **Canonical home:** [`control-flow.md`](control-flow.md) §5

---

## 3. Types, Storage, and Binding

### 3.1 place expression
- **Meaning:** A place expression denotes an existing, stable storage location. Almost every place may mint an `&` — a bare symbol, a field access of a place, an `&T` parameter — and only a `[]` expression is a place excluded from doing so (§3.36).
- **Why this name:** The term names the expressions that refer to a storage "place" rather than to a temporary value.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.2 value-downstream enforcement
- **Meaning:** A value type may contain only primitives and other value types, never a reference (`#`) or `&` field anywhere downstream in nested value-type fields. The rule turns on copying — a value is copied, and a reference type exists in order not to be. It does **not** bar recursion: a value type may lead back to itself through a boxed member (§3.39).
- **Why this name:** The rule is checked recursively through fields downstream from the outer value type, not just at the first field layer.
- **Canonical home:** [`memory.md`](memory.md) §2.10

### 3.3 unified type parameters
- **Meaning:** A type or number parameter is a *type parameter* (`name Type`, an uppercase name such as `T`, ranging over types) or a *number parameter* (`name Number`, a lowercase name such as `n`, ranging over compile-time numbers and resolving to a number value in body positions). A type definition declares its parameters in a `<>` header (their order is applied positionally at use sites); a verb — function, method, operator, constructor, or lambda — has no header and introduces each parameter inline within its value parameters, at the parameter's first marked occurrence. Parameters are referenced by bare name; casing carries the kind.
- **Why this name:** Type and number parameters share one concept-and-reference system (the `Type`/`Number` concepts, bare references, and the casing rule) across types and verbs; only the introduction site differs — a header for types, which are applied positionally, and inline for verbs, whose parameters are always inferred.
- **Canonical home:** [`generics.md`](generics.md) §3

### 3.4 compiler concept types
- **Meaning:** Compiler-provided types such as `@concepts$Number` may appear in parameter positions for literals but not in storage.
- **Why this name:** These are compiler-defined concept-level placeholders for source literals, not ordinary user storage types.
- **Canonical home:** [`syntax.md`](syntax.md) §2.8

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

### 3.14 `Type` and `Number` parameter concepts
- **Meaning:** `Type` and `Number` are compiler concept types used to declare type and number parameters (`T Type`, `n Number`). Like other concept types they are legal only in parameter positions, never as storage. As `()` value parameters they are passed explicitly; introduced inline on a verb parameter's type or nested type they are inferred; listed in a type's `<>` header they are applied positionally at use sites.
- **Why this name:** A type or size handed to a declaration is a compile-time value, so its parameter has a concept type like any other rather than a bespoke parameter-kind keyword.
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

### 3.21 member-versus-value delimiter
- **Meaning:** `;` terminates every member of a `struct`/`variant` body (and their `#` forms) and every arm of a `match` block, and is always trailing (newlines insignificant there); `,` separates the elements of a value collection (arrays, `enum`, call/constructor args, `init{}` fields, generic args, and the case list of a `match` group `[ … ]`) and is never trailing; a newline separates statements.
- **Why this name:** The delimiter is chosen by what is being separated — a declaration member versus a value-collection element versus a statement — so the name states the distinction the rule turns on.
- **Canonical home:** [`lexical.md`](lexical.md) §6

### 3.22 verb
- **Meaning:** A callable whose body is a sequence of statements that executes to do work. The verbs are functions, methods, operators, constructors, and lambdas (a lambda being an anonymous verb, the only verb that also has a value form). A subscript is **not** a verb: its body must be a place expression that projects existing storage rather than running computation, so it designates a place instead of executing.
- **Why this name:** The unifying trait is the executing statement body — a verb *does* something — which is why a constructor (statements ending in `return init{}`) counts and is indistinguishable from a builder helper apart from its `init{}` sugar, while a place-projecting subscript does not.
- **Canonical home:** [`functions.md`](functions.md) §1

### 3.23 anchor cell
- **Meaning:** An 8-byte runtime cell in the global anchor pool containing a `u32` target and a kind. A payload anchor targets a hosted object's segmented offset; a forwarding anchor targets another anchor after two hosting identities merge. A guest's `u32` tether names an anchor cell and follows forwarding cells until it reaches the terminal payload anchor.
- **Why this name:** The cell is a stable point through which an older guest identity can remain attached to a moving value, either directly or through another anchor.
- **Canonical home:** [`memory.md`](memory.md) §4.1

### 3.24 segmented-offset tether
- **Meaning:** The internal representation of a guest: a `u32` segmented offset pointing at an anchor cell, not a raw pointer. The cell may directly target the hosted payload or forward to another anchor. The value `0` means no tether. A tether is a runtime mechanism, distinct from the source-facing `&T` guest (§3.33).
- **Why this name:** The tether connects a guest's stored representation to the anchor through which it reaches the hosted object.
- **Canonical home:** [`memory.md`](memory.md) §4.2

### 3.25 arena placement
- **Meaning:** A scope's arena has two regions: statically sized **scope-level** storage — value slots, reference-type hosts, and the fixed-size handles materialized in those slots — is bump-allocated inline in the fixed-size region of the scope that creates it, while the payloads those handles name (resizable backing stores and boxed members, §3.39) go in that scope's dynamic region. A handle that sits *inside* a dynamic payload rather than in a scope slot — a boxed node's own boxed members, an element's owned storage — is part of that payload's block and is not separately placed. Rehosting copies the complete hosted representation into destination-owned storage: inline bytes move into the destination fixed-size region, each dynamic block is relocated into an equal-size destination-region allocation — recursively, through any blocks it owns in turn — and the old source storage ceases to be live. The source host-capable slot then stores the terminal tether as a guest. Placement is an unobservable implementation choice.
- **Why this name:** Placement is a choice among **arenas** — the per-scope regions — rather than between a stack and a heap; the creating scope's arena is the default, a parent arena the fallback on escape.
- **Canonical home:** [`memory.md`](memory.md) §3.5

### 3.26 capability marker
- **Meaning:** A surface marker on a verb that selects its kind and unlocks one capability: naming the first parameter `this` makes a method and grants private-field access; naming the verb after a type makes a constructor, implying its return type and unlocking `init{ }`; a symbol name makes an operator; no name makes a lambda. The parameter system, body grammar, overload resolution, and effect model are shared across all verbs.
- **Why this name:** The marker is a small piece of surface form that, by its presence, grants a *capability* to an otherwise-ordinary verb — so a constructor is a verb with one marker, not a separate mechanism.
- **Canonical home:** [`functions.md`](functions.md) §8

### 3.27 borrow
- **Meaning:** Non-hosting, non-escaping access to a caller's value storage for the duration of a call. It is how every value type is passed, and the only way one is passed: a value parameter is a read-only borrow, a value-type `mut` subject is a mutable borrow, and a value is copied only when bound into a fresh slot. A reference type is never borrowed — it is swallowed or guested (§3.37).
- **Why this name:** The callee is lent the caller's storage for the call and gives it back at return — it does not host it and cannot keep it. Unlike a guest, the borrow itself has no anchor or tether and cannot be stored, returned, or used as a move source — a restriction on the borrow, not on the value read through it, which a value type may still copy into a fresh slot.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.28 coercion site
- **Meaning:** A position where the compiler inserts an applicable implicit conversion automatically: a callable argument, a named field entry of a field-constructor call, a condition, or a counted-loop bound. It is *not* inserted where a value is written to a locally-fixed destination — a symbol declaration, an assignment or store, a `return`, or an `init{ }` — where the conversion is written explicitly.
- **Why this name:** "Coercion" is the standard term for an implicit, compiler-inserted type conversion, as opposed to an explicit cast; a *coercion site* names a position where that conversion is permitted. A coercion is backed by an `implicit` constructor, including the literal constructors supplied by the bundled `core` implementation — the site says where one may be inserted, not that arbitrary conversion is built in.
- **Canonical home:** [`types.md`](types.md) §4.2

### 3.29 mould
- **Meaning:** One of the three constructs that give a type its shape: `struct`, `variant`, and `enum`. Each has a value form and a `#` reference form, and a mould appears only as the right-hand side of a `type` or `alias` declaration, so every constructible type is named.
- **Why this name:** A mould gives shapeless material a fixed form, which is what these three do to a type; the word also carries that a mould is the form a type is cast from.
- **Canonical home:** [`types.md`](types.md) §5.3

### 3.30 value mould / reference mould
- **Meaning:** A mould is written in one of two forms: a **value form**, unmarked, or a **reference form**, carrying a leading `#`. The form decides whether the declared type is copied and transitively value, or identity-bearing, moved, and accessible through guests. It does not decide whether the type may recurse — both forms may, through a boxed member (§3.39).
- **Why this name:** The `#` mark names one axis — value versus reference — that crosses every mould.
- **Canonical home:** [`types.md`](types.md) §2.1

### 3.31 product mould / sum mould / peer mould
- **Meaning:** The three mould shapes, named by how a value's representations count. A `struct` is a **product mould** (its representations are the product of its fields'); a `variant` is a **sum mould** (the sum of its cases' payloads'); an `enum` is a **peer mould** — a flat set of payloadless, equal-rank peers, so its representations number exactly its members. `struct` and `variant` share one `{ }` body grammar; an `enum`'s peers are a flat `[ ]` list.
- **Why this name:** Product and sum are the standard algebraic names for the two `{ }`-bodied shapes; "peer" names the third from the `enum`'s own defining property — uniform, interchangeable, payloadless members — rather than forcing it into the sum family it degenerately belongs to.
- **Canonical home:** [`types.md`](types.md) §2.5 (product, sum); [`adt.md`](adt.md) §2 (peer)

### 3.32 host
- **Meaning:** A source-facing symbol, field, or container slot that stores a reference-type object — or its hosting handle — and governs that object's lifetime. This is the role commonly called an **owner** in other languages. Every reference-type object has exactly one host at a time. Moving the object transfers it to a new host.
- **Why this name:** Zane says **host** because a real-life host provides both accommodation and the duration of a guest's stay; the term emphasizes where an object resides and how long it remains available.
- **Canonical home:** [`memory.md`](memory.md) §2.1

### 3.33 guest
- **Meaning:** The source-facing `&T`: access to a hosted reference-type object without storing that object or controlling its lifetime. A guest may be repointed, copied when assigned or passed, stored in an `&` field or element, or returned as `&T`, but it cannot outlive its host. Every store of a guest, and every later store of a value that carries one (§3.42), compares owners (§3.43): what the guest names must have an owner that outlives the owner of the place holding it ([`lifetimes.md`](lifetimes.md) §1.1). It may be minted from any place but a `[]` expression (§3.36), and it names the object hosted there at that moment, travelling with that object if it is later moved. Internally, a guest is represented by a tether (§3.24) that resolves through an anchor cell (§3.23).
- **Why this name:** A guest may use what a host provides without owning it, and the guest's stay cannot outlast the host. The pair names the source relationship without exposing its runtime mechanism.
- **Canonical home:** [`memory.md`](memory.md) §2.4

### 3.34 swallowed parameter
- **Meaning:** A plain reference-type (`T`) parameter, which takes its argument by **hosting access** at the call-site scope. Passing a hosting value to a swallowing parameter downgrades the caller's symbol to a guest (§3.33), regardless of what the callee does with the value.
- **Why this name:** "Swallow" says the parameter takes the hosting value in; the caller's host goes in and is left holding only a guest.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.8

### 3.35 relay / consume
- **Meaning:** The two ways a verb can treat a reference-type host it swallows, told apart by its return. It **relays** the host when it returns a hosting handle; the caller may bind that return to host the object again. It **consumes** the host when it returns no hosting handle. A verb that declares `&T` instead takes a guest and leaves the caller's host unchanged.
- **Why this name:** "Consume" names taking the value for good; "relay" names passing the hosting role through and handing it back out.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.8

### 3.36 guest source
- **Meaning:** A place expression a new `&` may be minted from: a **bare symbol**, a field access whose base is a place, or an `&T` parameter. Only a `[]` expression is a place excluded, and temporaries are not places at all. The guest names the object hosted at that source when it is minted; if the object is moved the guest follows it, and if the object is destroyed by an overwrite of its slot the guest carries forward to the replacement.
- **Why this name:** The term names the *source* end — where a guest may come from — separately from what a guest survives once minted, which is the anchor system's business.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.37 passing mode
- **Meaning:** Which of two ways a reference-type argument reaches a callee, fixed entirely by the parameter's surface form: `T` **swallows** it (hosting access; the caller downgrades to a guest), `&T` takes a **guest** (readable, mutable, returnable, and storable, with a stored guest's resting place recorded in the signature and checked at each call; requires a guest source, which a bare symbol satisfies). The subject parameter (§3.38) has no such choice — it is always a guest — so `&` is never written on `this`. Two overloads may not differ only by the mode at one position.
- **Why this name:** "Mode" names a choice about *how* the same argument travels rather than *what* it is — the type is unchanged in both, and only the caller's obligations and resulting state differ.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.38 subject / subject parameter / subject expression
- **Meaning:** The **subject** is the object a method is called on. The **subject parameter** is `this`, the declaration's first parameter, always written bare: a reference-type subject is an implicit guest, a value-type subject a borrow (§3.27), and no marker is written on `this` for either. The **subject expression** is what stands left of `:` or `!` at the call site and supplies the object.
- **Why this name:** Grammar, matching `verb` (§3.22): a call reads *subject–verb–object*, and the subject is what the verb acts from. The three senses are one word in ordinary use because they usually coincide; the spec separates them where a rule holds of the declaration but not the object, or the other way round.
- **Canonical home:** [`functions.md`](functions.md) §2.1

### 3.39 boxed member
- **Meaning:** A **member** — a `struct`/`#struct` field or a `variant`/`#variant` case payload alike — stored as a fixed-size handle inline, with the instance it names placed in the scope's dynamic region. **Required** where the member's declared type can lead back to the enclosing type along **owning** edges — an `&` guest is not one — and **permitted** elsewhere. Two questions about it have two different answers: what the payload *is* follows the member's own declared type, while what *becomes of* it on move, copy, or death follows the enclosing type's kind. Nothing marks it in the source, and placement is unobservable (§3.25).
- **Why this name:** "Boxed" is the ordinary word for a value stored out of line behind a handle, and **member** rather than *field* because a `variant` case payload is boxed on the same terms as a `struct` field.
- **Canonical home:** [`adt.md`](adt.md) §4; representation in [`memory.md`](memory.md) §3.3 and §3.6

### 3.40 deep value copy
- **Meaning:** Copying an existing value copies the whole value — its inline bytes, plus a fresh allocation and recursive copy of the payload behind every boxed member (§3.39) it owns — so an original and its copy share no storage. A fresh non-place expression constructs directly in its destination instead of being copied there. Depth is what keeps a value transitively alias-free once it may own out-of-line storage, and so what lets a value type recurse (§3.2) and stay legal as a concurrent subject (§2.4).
- **Why this name:** "Deep" is the standard word for a copy that follows indirections instead of duplicating them, and the contrast it names — deep versus shallow — is precisely the choice the rule settles.
- **Canonical home:** [`memory.md`](memory.md) §2.3

### 3.41 move-source
- **Meaning:** An expression denoting a hosting value that the expression is entitled to consume, and therefore the only thing that may be moved into a hosting position. Three forms qualify: a **direct host symbol**; a **hosting verb result**, from a verb whose return type is a hosting `T`; and a **`#variant` case form**, `Variant.case(payload)` on a **reference** sum, which is built-in syntax rather than a verb but produces a fresh value nothing hosts yet. A *value* `variant` case form is not one — a value sum is copied rather than hosted, so there is no hosting to transfer. Neither is an `&` value, a value-type borrow, a field access, nor a container element access.
- **Why this name:** It names the *source* end of a move, which is where the restriction lives: the rule is about what an expression is entitled to give up, not about where the value lands.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.2

### 3.42 carried guest
- **Meaning:** An `&` reachable from a value's type by following **owning** edges — the same graph the boxed-member rule reads (§3.39). The walk finds an `&` member and stops there: the `&` itself is a carried guest, and a type's own `&` field is the shortest case, found after no edges at all. What the walk does not do is continue *through* the `&` into whatever it names, because that object is hosted elsewhere and travels separately. Every store of a value re-compares the owners (§3.43) of the hosts its carried guests name, which is why a check made where the value was first written does not have to survive the value moving. A guest naming a host **inside** the value is satisfied at every destination, because that host travels with it. A value carrying no guest is never re-checked.
- **Why this name:** The value *carries* the guest the way luggage carries its contents — the guest travels with it and is not part of what the value is used for, which is exactly why the store that relocates the value is the one that has to look inside.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.10

### 3.43 owner
- **Meaning:** The lifetime a place belongs to, and the only thing the store rule compares. A **symbol** is owned by the block that declares it; a **field or element** is owned by its root symbol's owner, never its own; a **parameter** (`this` included) and a constructor's `init{ }` have no owner in the body at all — each stands for a path in the caller's frame, so a store reaching one is settled at the call site. A block outlives every block nested within it, and a block is one lifetime rather than a sequence: everything it owns dies when it drains, with nothing to observe between. Hosts inside a stored value travel with it and take the destination's owner.
- **Why this name:** It names what a place's lifetime *is owed to* rather than where the place is written, which is the distinction the rule turns on — a field's own position tells you nothing, its root's owner tells you everything.
- **Canonical home:** [`lifetimes.md`](lifetimes.md) §1.1

---

## 4. Packages, Operators, and Versioning

### 4.1 home-package operator rule
- **Meaning:** A source operator implementation may be declared only in the home package of one of its user-defined operand types. Fundamental-only operators live in the bundled `core` implementation.
- **Why this name:** The rule ties operator declarations to the package that "owns" one operand type and prevents unrelated helper imports from changing operator meaning.
- **Canonical home:** [`operators.md`](operators.md) §2.2

### 4.2 placeholder-prefix rewriting
- **Meaning:** During fetch, a library's `!`-prefixed export symbols are rewritten with the resolved version tag before caching and linking.
- **Why this name:** The committed `!` prefix is only a placeholder marker; the toolchain rewrites that prefix into the real versioned symbol prefix.
- **Canonical home:** [`dependencies.md`](dependencies.md) §6.1

### 4.3 URL identity
- **Meaning:** A package's canonical identity is its full source URL, while local aliases are only import conveniences.
- **Why this name:** The rule says identity comes from the repository URL itself, not from whichever alias a project chooses locally.
- **Canonical home:** [`dependencies.md`](dependencies.md) §1 and §2
