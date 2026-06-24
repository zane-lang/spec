# Rationale: Foundations

Design journal for [`spec/foundations.md`](../spec/foundations.md). This file records the bets behind the language's foundational commitments — why each was chosen over its alternative, and what it costs. Where a foundation is worked out in one area, the deeper entry lives in that area's journal and this file links to it.

Entries are dated and append-only. When a decision changes, add a new entry rather than rewriting the old one — this file is a history of thinking, not a definition. The normative commitments live in the spec; nothing here overrides them.

> **See also:** [`writing-rationale-docs.md`](../contributing/writing-rationale-docs.md) for the format of this file. [`spec/foundations.md`](../spec/foundations.md) for the commitments these entries justify.

---

## Source is captured intent
**Spec:** [`foundations.md`](../spec/foundations.md) §2 · **Settled:** 2026-06

The fork is the oldest one in language design: meet the machine where it is, or meet the programmer where they are. A low-level language (C) makes the source a faithful transcript of machine steps — fast, but the intent is buried under bookkeeping. A high-level managed language (most of the rest) makes the source express intent — readable, but it pays for the altitude with a runtime that guesses at the mechanism the programmer elided.

Zane bets that this is a false choice *if the source is forced to be unambiguous*. The wager: a program that expresses intent cleanly leaves the compiler **more** freedom to choose the mechanism, not less — because nothing important was left implicit for a runtime to reconstruct. High level and fast are not opposed; they are both consequences of intent being captured well.

The catch, and the reason this is a real bet rather than a slogan, is the word *forced*. The altitude only pays off if expression is unambiguous, and programmers do not write unambiguously on their own. So the rest of the foundations (and most of the language's strictness) exist to make good expression mandatory. "Captured intent" is the goal; the strictness is the price of admission.

❌ Low-level by default (C model). Rejected: the intent is recoverable only by reverse-engineering the bookkeeping, which defeats the purpose and is exactly what makes optimization across the program hard.

❌ High-level with a managing runtime (GC, dynamic dispatch by default). Rejected: it buys readability by handing the mechanism to a runtime that must be conservative, which is the performance cost we are trying to avoid.

**Costs / deferred:** the bet only holds for *well-expressed* programs, and the language makes you express well by force. That is a real ergonomic cost — more up-front rules to satisfy than either a permissive low-level or a permissive high-level language demands. We are betting the payoff (readability and speed at once) is worth the stricter front door. The whole language is, in a sense, a test of that bet.

---

## Compilation is staged; types are values
**Spec:** [`foundations.md`](../spec/foundations.md) §3 · **Settled:** 2026-06

The decision to make types ordinary compile-time values executed in an earlier stage is the root that generics, the `<>`/`()` split, and compile-time arguments all grow from. The full argument — why staging beats a bolt-on generics sublanguage, and where the model currently overpromises (type-level arithmetic, constraints) — is recorded once, in the generics journal, rather than duplicated here.

> See [`generics.md`](generics.md) — "Types are templated functions" and "Deferred: what the model promises but does not yet deliver".

**Costs / deferred:** the elegance of "a type is just a function you run" oversells how much falls out for free. Type identity for computed type expressions, and constraints on parameters, do *not* fall out of it; they are the hard parts the framing hides. Tracked in the generics journal.

---

## Casing determines kind
**Spec:** [`foundations.md`](../spec/foundations.md) §4 · **Settled:** 2026-06

A name's kind has to be recoverable somehow — by the parser, to know whether `Foo<Bar>` is a type application or a comparison, and by the reader, to know whether `T` is a type or a value. There were three ways to encode it:

❌ Sigils (`&T`, `'a`, `@T`, `$x`). Rejected: every kind distinction becomes punctuation noise at every use site, and the surface stops reading like captured intent (§2).

❌ Keywords / declaration context only (infer kind from where a name was declared). Rejected: it makes the parser context-sensitive and forces the reader to remember a name's declaration to read a use — exactly the implicitness the language avoids.

The choice: encode kind in *case*, a property every identifier already has, costing zero extra tokens. An uppercase-initial name is a type; lowercase is a value. The parser and the reader recover kind from the same signal, everywhere, for free. This is what makes `<>` type syntax viable at all (`Vector<Int>` vs `a < b`) and what lets parameters drop their sigils (bare `T`, bare `n`).

**Costs / deferred:** case is no longer a free stylistic choice — you cannot name a type in lowercase or a local in uppercase, because case now carries meaning. Identifier conventions in other languages (SCREAMING_CONSTANTS, lowercase type aliases) are unavailable by construction. We judge a single, universal, zero-token kind signal worth the loss of naming latitude. The two-kind encoding (type vs value/number) is also load-bearing in a way that may strain if a third compile-time kind ever needs its own casing; noted in [`generics.md`](generics.md) under "Parameters are concept-typed (`Type` / `Number`)".

---

## Strictness is the performance model
**Spec:** [`foundations.md`](../spec/foundations.md) §6 · **Settled:** 2026-06

The tempting framing — the one to *reject* — is that Zane has two separable properties: it is strict (single ownership, fixed layout, enforced effects, mandatory error handling) *and* it is fast, as if these were independent selling points that happen to coexist.

They are not independent. The strictness is the *cause* of the speed. Every rule that forbids a convenience is the same rule that lets the compiler stop guessing: known ownership ⇒ deterministic destruction with no collector; fixed layout ⇒ direct access and cheap copies; known effects ⇒ safe automatic parallelism. This is the same trade Rust makes — you do not get speed from being expressive, you get it from the invariants the rules preserve. The expressiveness is what the invariants *buy back*.

This is why the language **forbids** rather than **discourages**. A guarantee that holds only when the programmer is careful is a guarantee the compiler cannot build on — it would have to emit the conservative code path anyway, for the cases where the programmer was not careful. Optionality would forfeit the entire benefit. The rule has to be total to be worth anything.

❌ Strict-by-default but with opt-out escape hatches everywhere. Rejected: each escape hatch reintroduces the conservative path the strictness was meant to eliminate, so the optimization can no longer be assumed. Narrow, clearly-marked `unsafe`-style boundaries are a separate question; *pervasive* opt-out is what is rejected here.

❌ Permissive with optimization left to the compiler. Rejected: without the invariants, the compiler is reduced to proving them by analysis, which is undecidable in general and conservative in practice. Better to have the language guarantee what the optimizer would otherwise have to prove.

**Costs / deferred:** this is the language's steepest cost. Mandatory strictness means a higher learning curve and more programs the compiler rejects that *would* have run fine — the price of forbidding a convenience is that you cannot use it even when it would have been safe in your particular case. We are betting that the floor it puts under performance and reasoning is worth the constructs it takes off the table. It is the most expensive bet in the language and the one most worth revisiting if the ergonomic cost proves too high in practice.

---

## A foundations doc, separate from the philosophy that justifies it
**Spec:** [`foundations.md`](../spec/foundations.md) (whole) · **Settled:** 2026-06

A small meta-note, since this file's own existence is a design decision. The cross-cutting "why" of the language could have lived in one essay. We split it: the *model* (staging, casing, fixed layout, the strictness principle) is normative and lives in `spec/foundations.md`, because topic docs cite it as ground truth; the *bets and costs* (this file) are journal material, because they are opinion, history, and honestly-stated downside.

❌ A single `philosophy.md` holding both. Rejected: it would mix normative grounding that other docs must be able to reference as fact with editorial argument that should be free to be revised and second-guessed — the exact what/why blend the spec/rationale split exists to prevent.

**Costs / deferred:** a reader after "the big picture" now has two files to read instead of one. Mitigated by the cross-links: the spec doc states each commitment and points here for the argument. We accept the extra hop for the same reason the whole split exists — a definition and the debate behind it have different lifecycles and should not share a page.
