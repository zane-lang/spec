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
- **Meaning:** A spawned call may mutate only a value-typed receiver, and at most one live spawn may mutably borrow a given storage location. A value type is transitively alias-free, so the rule rules out an aliased data race from the receiver's type alone; concurrent reads take a coherent snapshot instead of serializing.
- **Why this name:** Concurrent mutation is gated on the receiver being a value type — the property that makes race-freedom checkable without whole-program alias analysis.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.2 and §4.3

### 2.5 water-tower lifetimes
- **Meaning:** Scope-owned objects stay alive until every `spawn`ed call in that scope has completed and the scope drains.
- **Why this name:** The source document explains the rule through a water-tower analogy in which each still-running spawned call acts like a plate holding the water level up.
- **Canonical home:** [`concurrency.md`](concurrency.md) §4.1

### 2.6 structural effect model
- **Meaning:** Effect level is inferred from `mut`, ownership, call structure, and reachable capabilities rather than from a large set of written annotations.
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
- **Meaning:** A place expression denotes an existing, stable storage location. Some place expressions may create new `&` values, while `[]` expressions remain excluded from that rule.
- **Why this name:** The term names the expressions that refer to a storage "place" rather than to a temporary value.
- **Canonical home:** [`memory.md`](memory.md) §2.8

### 3.2 value-downstream enforcement
- **Meaning:** A value type may contain only primitives and other value types, never a reference (`#`) or `&` field anywhere downstream in nested value-type fields.
- **Why this name:** The rule is checked recursively through fields downstream from the outer value type, not just at the first field layer.
- **Canonical home:** [`memory.md`](memory.md) §2.10

### 3.3 unified type parameters
- **Meaning:** A type or number parameter is a *type parameter* (`name Type`, an uppercase name such as `T`, ranging over types) or a *number parameter* (`name Number`, a lowercase name such as `n`, ranging over compile-time numbers and resolving to a number value in body positions). A type definition declares its parameters in a `<>` header (their order is applied positionally at use sites); a verb — function, method, or constructor — has no header and introduces each parameter inline within its value parameters, at the parameter's first marked occurrence. Parameters are referenced by bare name; casing carries the kind.
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
- **Why this name:** Privacy is granted by the method/receiver relationship, not by where the function is declared.
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
- **Meaning:** An expression that names a scrutinee and a `{ }` block of `;`-terminated arms; each arm has an optional binder, a case (or `[ ]` group of cases) selector, and a body. It dispatches on the live tag, is exhaustive with no default arm, all arms share one return type, and abort flows through.
- **Why this name:** "Match" is the familiar name for tag-directed selection, here surfaced as a single central block over a variant's cases.
- **Canonical home:** [`adt.md`](adt.md) §5

### 3.20 enum map property
- **Meaning:** A package-scope, exhaustive, access-only declaration that attaches uniform external data to an enum's members and is read field-style (`Colors.red.colorName`). It is not a passable value; its result is a value.
- **Why this name:** It maps each enum member to a value of a named property, and it is named where the value is read, so "enum map property" describes both the table and its access form.
- **Canonical home:** [`adt.md`](adt.md) §6

### 3.21 member-versus-value delimiter
- **Meaning:** `;` terminates every member of a `struct`/`variant` body (and their `#` forms) and every arm of a `match` block, and is always trailing (newlines insignificant there); `,` separates the elements of a value collection (arrays, `tuple`, `enum`, call/constructor args, `init{}` fields, generic args, and the case list of a `match` group `[ … ]`) and is never trailing; a newline separates statements.
- **Why this name:** The delimiter is chosen by what is being separated — a declaration member versus a value-collection element versus a statement — so the name states the distinction the rule turns on.
- **Canonical home:** [`lexical.md`](lexical.md) §6

### 3.22 verb
- **Meaning:** A callable whose body is a sequence of statements that executes to do work. The verbs are functions, methods, operators, constructors, and lambdas (a lambda being an anonymous verb, the only verb that also has a value form). A subscript is **not** a verb: its body must be a place expression that projects existing storage rather than running computation, so it designates a place instead of executing.
- **Why this name:** The unifying trait is the executing statement body — a verb *does* something — which is why a constructor (statements ending in `return init{}`) counts and is indistinguishable from a builder helper apart from its `init{}` sugar, while a place-projecting subscript does not.
- **Canonical home:** [`functions.md`](functions.md) §1

### 3.23 anchor cell
- **Meaning:** A single `u32` holding the current segmented offset of one tethered owner — a stable indirection point that tethers read through. It is bump-allocated on the first tether in a dedicated anchor-cell region of the owner's scope arena, kept separate from payloads so payload iteration stays dense. There is no monolithic table: each tethered owner has its own cell.
- **Why this name:** The cell *anchors* a tethered owner: a fixed point a drifting value stays tethered to, so a move updates the one cell and every tether follows.
- **Canonical home:** [`memory.md`](memory.md) §4.1

### 3.24 segmented-offset tether
- **Meaning:** A tether is represented as a `u32` **segmented offset** — a chunk id in the high bits and an in-chunk word offset in the low bits — pointing at the owner's anchor cell, not a raw pointer. The value `0` (chunk `0`, word `0`) is the reserved null location and means untethered.
- **Why this name:** The tether is a *segmented offset*: half a pointer's size, resolved through the chunk directory to an address, and stable across arena teardown because it names a chunk-relative position rather than a native pointer.
- **Canonical home:** [`memory.md`](memory.md) §4.2

### 3.25 arena placement
- **Meaning:** A reference-type instance is bump-allocated in the arena of the scope that creates it, and is copied (promoted) into a parent arena only if it escapes that scope. Only dynamic size or escape changes where an instance lives. Placement is an unobservable implementation choice.
- **Why this name:** Placement is a choice among **arenas** — the per-scope bump regions — rather than between a stack and a heap; the creating scope's arena is the default, a parent arena the fallback on escape.
- **Canonical home:** [`memory.md`](memory.md) §3.5

### 3.26 capability marker
- **Meaning:** A surface marker on a verb that selects its kind and unlocks one capability: naming the first parameter `this` makes a method and grants private-field access; naming the verb after a type makes a constructor, implying its return type and unlocking `init{ }`; a symbol name makes an operator; no name makes a lambda. The parameter system, body grammar, overload resolution, and effect model are shared across all verbs.
- **Why this name:** The marker is a small piece of surface form that, by its presence, grants a *capability* to an otherwise-ordinary verb — so a constructor is a verb with one marker, not a separate mechanism.
- **Canonical home:** [`functions.md`](functions.md) §8

### 3.27 borrow
- **Meaning:** Non-owning, non-escaping access to a caller's storage for the duration of a call — the passing mode for **value types**, which have no `&` of their own. A value parameter is a read-only borrow and a value-type `mut` receiver is a mutable borrow; a value is copied only when bound into a fresh slot. Reference types are passed by `&` or swallowed instead, and a reference-type `this` is an implicit `&`.
- **Why this name:** The callee is lent the caller's storage for the call and gives it back at return — it does not own it and cannot keep it. Unlike an `&`, a borrow has no anchor and cannot be stored or returned, which is what lets a value be mutated in place without becoming aliasable.
- **Canonical home:** [`memory.md`](memory.md) §2.9

### 3.28 coercion site
- **Meaning:** A position where the compiler inserts an applicable `implicit` constructor automatically: a positional argument of a function or constructor call, or a named field entry of a field-constructor call. It is *not* inserted where a value is written to a locally-fixed destination — a symbol declaration, an assignment or store, a `return`, or an `init{}` — where the conversion is written explicitly.
- **Why this name:** "Coercion" is the standard term for an implicit, compiler-inserted type conversion, as opposed to an explicit cast; a *coercion site* names a position where that conversion is permitted. Each coercion is still backed by a user-declared `implicit` constructor — the site says where one may be inserted, not that the conversion is built in.
- **Canonical home:** [`types.md`](types.md) §4.2

### 3.29 mould
- **Meaning:** One of the four constructs that give a type its shape: `struct`, `variant`, `enum`, and `tuple`. Each has a value form and a `#` reference form, and a mould appears only as the right-hand side of a `type` or `alias` declaration, so every constructible type is named. The reach is total: even a core type such as `Int` is declared with a mould — `Int`, `Float`, and `Bool` are wrapper `struct`s over machine-storage primitives in the `@primitives$` namespace (see [`syntax.md`](syntax.md) §2.1). A mould shapes a type; the type is what it declares.
- **Why this name:** A mould gives shapeless material a fixed form, which is what these four do to a type; the word also carries that a mould is the form a type is cast from. The `#` value/reference axis applies to every mould uniformly ([`types.md`](types.md) §2.1); `struct` and `variant` share one `{ }` body grammar, while `enum` and `tuple` are `[ ]` lists.
- **Canonical home:** [`types.md`](types.md) §5.3

### 3.30 value mould / reference mould
- **Meaning:** A mould is written in one of two **forms**: a **value form**, unmarked, or a **reference form**, carrying a leading `#`. A mould in its value form is a **value mould** (`struct`, `variant`, `enum`, `tuple`); in its reference form, a **reference mould** (`#struct`, `#variant`, `#enum`, `#tuple`). The form decides the kind of the type the mould declares: a value mould declares a *value type* (copied, transitively value); a reference mould declares a *reference type* (identity-bearing, aliasable through `&`, may recurse).
- **Why this name:** The `#` mark names one axis — value versus reference — that crosses every mould, so each mould has two forms; "form" names the position on that axis and "value/reference mould" names a mould occupying it. Because a mould shapes a type, a *reference mould* stays distinct from the *reference type* it declares.
- **Canonical home:** [`types.md`](types.md) §2.1

### 3.31 product mould / sum mould
- **Meaning:** The two moulds that share one `{ }` body grammar, told apart by keyword. A `struct` is the **product mould**: a value of the type it declares has *all* its members at once, so that type is a **product type**. A `variant` is the **sum mould**: a value has *exactly one* member at a time, so that type is a **sum type**. `enum` and `tuple` are moulds of other shapes and keep their own names.
- **Why this name:** Product and sum are the standard names for the two shapes; pairing each with "mould" names the *construct* (parallel to value mould / reference mould, §3.30), while "product type" / "sum type" name the *type* the mould declares — so construct and type stay distinct.
- **Canonical home:** [`types.md`](types.md) §2.5

### 3.32 tether
- **Meaning:** The `&` value: non-owning storage that points at a **reference type**, resolved through the owner's anchor cell (§3.23) to the owner's current address. A tether is repointable, is copied when assigned or passed, may be stored in an `&` field or returned as `&T`, and carries no ownership — the owner's lifetime bounds the tether, never the reverse. It is represented as a `u32` segmented offset (§3.24), half the size of a pointer. A tether is distinct from the **reference type** (§3.30) it points at: the reference type is the kind of the owner, the tether is the non-owning handle to it.
- **Why this name:** A tether fastens a holder to a fixed point without owning it and can be re-tied elsewhere — exactly how an `&` attaches to an anchored owner and may be repointed. What a tether is fastened to also *bounds* it, which mirrors the rule that an owner's lexical scope bounds every tether taken on it (see [`lifetimes.md`](lifetimes.md) §1.1). It pairs with **anchor** (§3.23): the anchor holds fast, and the tether reads through it.
- **Canonical home:** [`memory.md`](memory.md) §2.4

---

## 4. Packages, Operators, and Versioning

### 4.1 home-package operator rule
- **Meaning:** An operator implementation may be declared only in the home package of one of its operand types.
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
