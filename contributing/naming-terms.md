# Naming Zane Terminology

This guide describes how the spec chooses the coined terms it reuses — the named
concepts recorded in [`glossary.md`](../spec/glossary.md), such as `verb`,
`mould`, `borrow`, and `anchor`. It governs the *terms of art* the documentation
leans on, not the surface keywords of the language itself.

Terminology is worth naming deliberately because a good term is used on nearly
every page: it is the handle a reader carries the concept around by. A term that
fights its reader — one that already means something else, or that says too much,
or too little — taxes every sentence it appears in.

---

## 1. A Term Is a Metaphor

Prefer to name a concept by borrowing an existing word whose ordinary job is the
same shape as the concept's job. The reader already owns the word; the analogy
does the teaching before the definition is even read.

- **`verb`** — a function, method, operator, constructor, or lambda. In grammar
  a verb is the word that *acts*; a callable is the construct that *does* work.
- **`mould`** — a `struct`/`variant`/`enum`/`tuple` form. A mould gives shapeless
  material a fixed form; these forms give a type its shape, and the type is what
  is cast from them.
- **`borrow`** — the passing mode for a value type. The callee is *lent* the
  caller's storage for the call and must give it back; it cannot keep it.
- **`anchor`** — the stable indirection a ref reads through. An anchor is a fixed
  point that holds something in place while the world around it moves.

In each case the everyday meaning is not decoration — it is a true structural
analogy. The word's real-world role maps onto the concept's role, so the name
reinforces the semantics instead of merely labelling them.

---

## 2. What Makes a Term Good

Weigh a candidate against all of these, not just the first.

### 2.1 The analogy is true, not decorative
The word's literal job should match the concept's job. A `mould` shapes; a `verb`
acts; a `borrow` is returned. If the only link is a vague vibe, the name will not
hold up as the concept is used in anger.

### 2.2 It is empty enough to refill
A term is a word you are going to overwrite with a new meaning through use. That
only works if the reader's prior claim on the word is weak enough to yield. `verb`
works because "action word" *feeds* the new meaning; a word already carrying a
strong, competing meaning in programming will *fight* it, and you will spend the
term's whole life pushing the old meaning out of the way. This is the usual
reason to reject an otherwise-apt candidate — see the `matrix` case in §5.

### 2.3 It reads in dense prose
A term appears many times per page, so it must be short, plain, and unremarkable
in a sentence. Everyday words win here. A term that is a mouthful, or that a
reader has to sound out, is a poor handle no matter how precise.

### 2.4 An oblique connection is fine
The link between the word and the concept may be one hop away; it need not
encapsulate the definition. A name is not a summary. `anchor` does not spell out
"stable indirection through an anchor table" — it just points, and the meaning
settles onto it with use. Aim for *connected but not descriptive*.

### 2.5 The meaning accrues through use
Do not expect the name to carry the whole concept on day one. A good term is a
little empty at first and fills up as the spec uses it. The best connections are
the ones a reader discovers *after* the word already feels natural — the buried
resonance that rewards a second look rather than announcing itself.

---

## 3. The Test

Before adopting a term, ask:

> Does this word **fight** its old meaning or **feed** it?

If a fresh reader's existing sense of the word points *toward* the concept, the
name will teach. If it points *away* — or toward a rival meaning the reader holds
more strongly, especially a programming one — reject it, however clever the
analogy.

---

## 4. Two Registers: Projects vs. Terms

The instinct to name things after Greek or Roman figures, places, or other
evocative-but-arbitrary words belongs to **project names**, not terminology. A
project name (a tool, a repository) can afford to be opaque and mythic — it is
seen rarely and gains its meaning slowly, so an oblique reference like *Ariadne*
(the thread through the labyrinth) is a strength.

A **term** is the opposite case: read constantly, and needed to teach on contact.
Terms therefore lean plain and everyday — `verb`, `mould`, `borrow`, `anchor` —
even when the underlying instinct (name by metaphor, keep the link oblique) is
the same. When in doubt for a term, choose the ordinary word over the exotic one.

---

## 5. Worked Example: `mould`

The concept: the four constructs that give a type its shape — `struct`,
`variant`, `enum`, `tuple` — which had been spelled out longhand as
"type-defining expressions" with no single name.

- **`origin form`** — accurate (every type originates in one of the four) but
  flat and descriptive; it *states* the definition rather than lending an image.
  Fails §2.4: a name, not a summary.
- **`matrix`** — the buried gem: in typography a *matrix* is the mould a piece of
  metal *type* is cast in, so it ties straight to the word "type" the language
  already uses. But it fails §2.2 hard — in programming `matrix` already means a
  math object, a DI container, and more. It would fight its old meaning on every
  line.
- **`arche`** (Greek "origin/first principle") — strong on meaning and quietly
  the root of *arche*type and *arch*itecture, but it fails §2.3: too exotic to
  read naturally on every third line of a spec, and wrong register for a term
  (§4).
- **`mould`** — keeps the exact metaphor `matrix` carried (the form a type is
  cast from) with none of the baggage. A programmer has essentially no prior
  claim on the word, so it refills cleanly (§2.2); it is short and ordinary
  (§2.3); and it pairs with `verb` in register — verbs act, moulds shape.

`mould` wins by doing what `matrix` did and then getting out of the way.

---

## 6. Recording a Term

Every coined term gets an entry in [`glossary.md`](../spec/glossary.md) with its
preferred label, a short meaning, *why that name was chosen*, and the canonical
home document for the full rule (see
[`writing-spec-docs.md`](writing-spec-docs.md) §2.6). Keep the glossary's "why
this name" to the short version. The developed reasoning — the candidates weighed
and rejected, the argument for the winner — is design history and belongs in the
matching `stories/` doc, not in the spec (see
[`writing-stories-docs.md`](writing-stories-docs.md)).
