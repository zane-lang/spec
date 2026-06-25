# Zane Foundations

This document states the few ideas the rest of the specification rests on. Every topic document assumes them; this one names them in a single place and points to where each is specified in full. It introduces no rules of its own — each foundation is owned canonically by another document, and this is the map that shows how they fit together.

> **See also:** [`lexical.md`](lexical.md) §3 for the casing rule. [`generics.md`](generics.md) §2 and §7 for types-as-functions and uniform stride. [`memory.md`](memory.md) §3 for layout. [`effects.md`](effects.md) for how the strictness is enforced.

> **Rationale:** [`rationale/foundations.md`](../rationale/foundations.md) tells the story behind these commitments — the bets they represent and the costs they accept.

---

## 1. Overview

Zane is built on a small number of commitments that the rest of the language derives from. They are not features; they are the ground the features stand on. Read this document first, then read any topic document as a working-out of these ideas in one area.

- **`Source is captured intent`.** Zane code is a high-level description of architecture and thought, not a transcript of machine steps. It is intentionally high-level because that is the form in which intent is clearest.
- **`Compilation is staged; types are values`.** A type is an ordinary compile-time value that the compiler *executes* in an earlier stage to produce a layout. Parameterization, and the split between type expressions and calls, fall out of this.
- **`Casing determines kind`.** A name's case tells the reader and the parser what kind of thing it is — a type or a value. This single rule is what makes the surface grammar viable without sigils.
- **`Layout is fixed`.** Every value of a given type is the same number of bytes. Uniform stride is a global invariant, not a per-type choice.
- **`Strictness is the performance model`.** The language forbids high-level conveniences that would dissolve a low-level guarantee. The rules are not a tax paid alongside fast code — they are the mechanism that produces it.

---

## 2. Source Is Captured Intent

Zane source is meant to read as a captured *idea* — the shape of a program's architecture and the thoughts behind it — rather than a step-by-step encoding of how a machine should proceed. This is why the surface is deliberately high-level: intent is clearest when it is expressed directly, not buried under manual bookkeeping.

The high level is not in tension with performance. Because the source carries *intent* rather than *mechanism*, the compiler is left with more freedom to choose the mechanism — provided the intent was expressed well. Expressing it well is not optional; the language's rules (the rest of this document) force enough structure that the captured intent is unambiguous. A well-expressed Zane program is therefore both easier to read and easier to compile to fast code, for the same reason: nothing important was left implicit.

This framing recurs throughout the spec. The two parallel document sets — `spec/` for *what* and `rationale/` for *why* — are the same idea at the level of the specification itself: capture the definition and the intent behind it, separately and explicitly.

---

## 3. Compilation Is Staged; Types Are Values

A type in Zane is an ordinary compile-time value. The compiler runs in stages: an earlier stage *executes* types — applying a parameterized type to its arguments to produce a concrete layout — before a later stage constructs and runs values. A type definition is, quite literally, a function from parameters to a layout.

Three consequences that other documents depend on:

- **Parameterization is not a separate feature.** It is what you get once a type is an executable value. A parameterized type lists its parameters and produces a result; applying arguments evaluates it. See [`generics.md`](generics.md) §2.
- **`<>` and `()` are different mechanisms, not two call syntaxes.** `<>` is a type expression, resolved in the earlier (type) stage; `()` is a construction or call, resolved in the later (value) stage. They live in different stages, which is why a call never carries a `<>` list. See [`generics.md`](generics.md) §4–§5.
- **A value passed at compile time is just an argument.** A type or compile-time number handed to a constructor is an ordinary value the body can use, because the stage that runs the type has those values in hand. See [`generics.md`](generics.md) §5.3.

> **Rationale:** [`rationale/generics.md`](../rationale/generics.md) — "Types are templated functions" records why staging was chosen over a bolt-on generics sublanguage, and where the model promises more than it currently delivers.

---

## 4. Casing Determines Kind

A name's initial case is semantic. An uppercase-initial name is a type; a lowercase-initial name is a value (including a compile-time number). The full rule, and its effect on parsing, are owned by [`lexical.md`](lexical.md) §3 and §5.

This is a foundation, not a style convention, because the whole surface grammar leans on it:

- `Vector<Int>` parses as a type application and `a < b` as a comparison, told apart solely by the case of the token before `<`. Without this, `<>` type syntax would not be viable. See [`lexical.md`](lexical.md) §5.
- A parameter needs no sigil. A bare `T` is a type and a bare `n` is a number, so a reference carries its kind without decoration. See [`generics.md`](generics.md) §3.
- The reader gets the same information the parser does: kind is visible at every use site, everywhere, for free.

The cost — that case is not a free naming choice — is accepted deliberately; see the rationale.

---

## 5. Layout Is Fixed

Every value of a given type occupies the same number of bytes. Size is part of the type, not a runtime property of the value. This uniform-stride invariant is global: it holds for every type, all the way up every containment chain.

It is the foundation under a large part of the runtime model:

- Indexing is `base + i * stride` with a compile-time `stride`; copying, struct embedding, and calling conventions all assume a fixed size. The full argument is in [`generics.md`](generics.md) §7.
- A type whose size could vary would propagate that variability into everything that contains it, so the invariant has to be global to be worth anything.
- Consequences elsewhere are derivations of this rule, not separate decisions: an array bakes its length into its type ([`generics.md`](generics.md) §8), and a directly self-referential value type is illegal because it would have infinite size, which is why recursion must box through `&` ([`adt.md`](adt.md) §4, [`memory.md`](memory.md) §2.10).

---

## 6. Strictness Is the Performance Model

Zane is strict — single ownership, fixed layout, enforced effect levels, mandatory error handling — and the strictness is not a separate concern from its performance. It *is* the performance story.

High-level expression, on its own, usually costs speed. What buys it back is that the rules preserve enough invariants for the compiler to generate good code without guessing: ownership is known, so destruction is deterministic and needs no collector; layout is fixed, so access is direct; effects are known, so independent work can be parallelized. Each rule that forbids a convenience is the same rule that licenses an optimization.

So the rules should not be read as a usability tax levied next to the performance. They are the bargain itself: *give up the conveniences that would force the compiler to be conservative, and in exchange the compiler can be aggressive.* This is why the language forbids, rather than merely discourages, the constructs that would dissolve a guarantee — a guarantee that holds only sometimes is one the compiler cannot rely on. The enforcement mechanisms live in [`memory.md`](memory.md), [`effects.md`](effects.md), and [`lifetimes.md`](lifetimes.md); this section is only the principle that unifies them.

> **Rationale:** [`rationale/foundations.md`](../rationale/foundations.md) — "Strictness is the performance model" weighs this bet against the permissive alternative and records its real ergonomic cost.

---

## 7. Summary

| Foundation | Commitment | Owned by |
|---|---|---|
| Captured intent | Source expresses architecture and intent, not machine steps; high-level on purpose | this document |
| Staged compilation | Types are compile-time values executed in an earlier stage; `<>` and `()` are different stages | [`generics.md`](generics.md) §2, §4–§5 |
| Casing determines kind | A name's case is its kind — uppercase type, lowercase value/number | [`lexical.md`](lexical.md) §3, §5 |
| Fixed layout | Every value of a type is the same size; uniform stride is global | [`generics.md`](generics.md) §7, [`memory.md`](memory.md) §3 |
| Strictness is performance | Forbidding guarantee-dissolving conveniences is what licenses aggressive codegen | [`memory.md`](memory.md), [`effects.md`](effects.md), [`lifetimes.md`](lifetimes.md) |
