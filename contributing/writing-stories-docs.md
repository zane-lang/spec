# Writing Zane Stories Docs

This guide describes the conventions for writing and maintaining **stories documents** in [`stories/`](../stories/). Follow it when adding a new stories document or extending an existing one. Its sibling, [`writing-spec-docs.md`](writing-spec-docs.md), governs the normative spec documents in [`spec/`](../spec/).

---

## 1. What a Stories Doc Is

The spec answers *what the language does*. A stories doc answers *how it came to do that* — the reasoning, told as it actually unfolded. The two are kept in separate folders because they have different jobs and different audiences.

A stories doc is **not normative**: nothing in it overrides a spec rule. The spec is the source of truth; a stories doc explains the thinking behind it. If the two ever disagree, the spec wins and the story is what gets fixed.

The reasoning lives in its own folder, rather than in a table at the foot of each spec doc, for one reason: **a table flattens.** Genuinely complex reasoning — the alternatives weighed, the tension resolved, the cost accepted — does not fit a one-sentence cell. A separate file lets the reasoning have the room it needs, and lets the spec stay terse.

**A stories doc reads like a history, not like a spec.** A spec entry is a rule. A story is the account of how that rule came to be — the situation that forced a choice, the roads not taken, the reasoning that settled it, and the cost it carries. Crucially, the unit is **not the individual decision.** Decisions do not exist as tidy, separable atoms; they crystallize out of a line of reasoning, in response to a pressure, often several at once. Writing one short justification per spec section pretends otherwise — it pretends the author was thinking decision-by-decision when they were really following a thread. A stories doc follows the thread instead, and tells it as **one continuous narrative**, not as a stack of self-contained entries.

---

## 2. Document Shape

A stories document is a title, an optional pointer to the spec, and then **one continuous narrative** that moves through the design in the order the thinking took. It has **no `---` separators and no per-decision headings** — it reads as a story, not as a sequence of chapters, each one flowing into the next on the thread of cause and consequence.

```
# Stories: <Topic>

> **See also:** [`spec/<topic>.md`](../spec/<topic>.md) — the rules this story explains.

<the narrative — flowing prose from the first pressure to the last. It opens on
the situation that forced the first choice, tells what was decided in response
(possibly several things at once), the roads not taken, and the cost; then moves
to the next pressure that choice created, and so on. One decision creates the
problem the next one solves, and the prose carries that lineage with explicit
transitions rather than headings. Links to specific spec points by permalink
when it discusses them (§4).>
```

Do **not** open the file with a paragraph explaining what stories documents are or how they work — that is this guide's job, not the file's. A reader who opens `stories/generics.md` wants the generics story, not a preamble. Start with the title, an optional `> **See also:**` line pointing at the matching spec doc, and then go straight into the narrative.

### 2.1 File name and location

A stories document **mirrors its spec document's file name**: the story behind [`spec/generics.md`](../spec/generics.md) lives at [`stories/generics.md`](../stories/generics.md). One stories file per spec file. This makes discovery mechanical — same name, sibling folder — and gives the anti-drift habit a simple form: *editing `spec/X.md`? check `stories/X.md`.*

Do not create one file per decision (too fragmentary) or one file for the whole language (unnavigable). The cross-cutting, language-wide *why* belongs in [`stories/foundations.md`](../stories/foundations.md); a topic stories doc cross-references it rather than restating it.

### 2.2 The thread, not the decision

The narrative is organised around **phases of the design** — episodes in which one pressure was met and resolved — but those phases are *not* marked off with headings or separators. They are joints in the prose, not sections on the page. A single episode may settle several decisions at once, because that is how they were actually made, and it should, when they were made together. Conversely, one large decision may run across several paragraphs if the thinking really arrived in stages. Let the joints fall where the *reasoning* has joints, not where the spec has section numbers.

The narrative is **not pinned to the spec's section structure.** It is free to range across whatever each episode touched, and to move in the order the thinking moved rather than the order the spec lists rules. It *should* link to specific spec rules where it discusses them (§4), but those links serve the reader; they are not a structural anchor, and the narrative is never reshaped just to line up one-to-one with a `§N`.

---

## 3. Writing the Narrative

Each episode within the narrative tends to move through four things — but as *prose*, never as labelled sections:

- **The situation.** What pressure forced a choice here at all — the cause. A passage that records no real pressure is probably not telling a story; it is restating a rule, and the spec already has it.
- **The roads not taken.** The alternatives, and why each lost. This is the highest-value part — it is precisely what a spec cannot hold — so do not skip it, but tell it in sentences ("We could have done X, the way C++ does, but…"), not as a bulleted list of rejects.
- **The resolution.** What was decided, and the reasoning that settled it.
- **The cost.** What the decision makes worse, what it defers, what it leaves open. An honest story always reaches this; a passage that only lists upsides is incomplete, and the cost is the part later readers will most want to have been told.

Let the length flex with the episode: a minor turn is a paragraph, a foundational one runs to pages.

**Order the narrative by the path the thinking took**, not by spec section order and not by tidy importance. Each episode usually opens where the previous one left off — one choice creates the problem the next one solves — so the natural order is causal and roughly chronological, and the transitions between episodes should make that lineage explicit ("With identity settled, the next question is…", "Shipping prebuilt objects raises a problem the moment…"). "When" here means *relative to the other decisions* ("once we had settled X, the next pressure was Y"), not a calendar date; record a real date or commit only if it genuinely matters. Recording the discarded attempts in the order they were tried is often clearer than a tidy after-the-fact summary: it shows *why* the final design has the shape it does, and it stops a future reader from re-walking roads already known to be dead.

---

## 4. Linking to the Spec

A stories doc is joined to the spec, and its episodes are joined to each other. Two kinds of link do two different jobs, and they are written differently.

### 4.1 The companion pointer (living)

The top-of-file `> **See also:**` line, and the spec's own `> **Story:**` pointers back (§4.4), are **navigational**: they say "here is the companion document." A reader following one wants the *current* doc, so these stay ordinary repo-relative links (`../spec/<topic>.md`, `../stories/<topic>.md`). They are expected to track the files as they move.

### 4.2 In-prose spec references (point-in-time → permalink)

When the narrative says *"the spec says X"* — when it quotes or leans on a specific rule as it stood when that passage was written — that is a **point-in-time claim**, and it must be a **permalink** pinned to a commit, not a moving relative link:

```markdown
[`lexical.md` §5](https://github.com/zane-lang/spec/blob/<commit-sha>/spec/lexical.md#5-how-casing-disambiguates-the-grammar)
```

The reason is the update model (§5). A story accumulates; a passage written today describes the spec *as it is today*. A relative link would silently re-point as the spec changes, so an old passage would end up "citing" a rule that no longer says what it claims — invisible drift, exactly the failure the anti-drift discipline exists to prevent. Pinning the link to the commit freezes the citation to the spec the passage was actually written against, so a later reader can always see what was true *then* and judge the reasoning on its own terms.

Use a full GitHub blob permalink with a commit SHA (`/blob/<sha>/...`), not a branch or tag name — branches and tags move, which defeats the point. Pin to the latest commit that holds the spec text the passage describes; for a passage written alongside a spec change, that is the commit that change lands in. The visible link text should still name the file and section (`` `lexical.md` §5``) so the citation reads naturally; the SHA rides in the URL.

Linking like this is **recommended wherever the narrative discusses a specific rule**, but it is not mandatory and the prose is never bent to accommodate one. A passage with no spec link is fine; the companion pointer (§4.1) already connects the reader to the doc.

### 4.3 Internal references within the story

Because the story is one continuous narrative, its episodes refer to each other **in prose**, not by heading anchor — there are no headings to anchor to. When one passage builds on an earlier choice, name it in the sentence and let the order of the narrative carry the link: *"Side-by-side coexistence is safe and simple, but…"* picking up a mechanism established paragraphs before, or *"the remapping opt-in, much later in this story, is written in URLs rather than keys"* pointing forward. The continuous order is what makes this work: a reader meets each idea before the passage that leans on it, so a back-reference is a phrase, not a link.

A reference to a *different* story file is an ordinary repo-relative link to that file (no anchor): `[the generics story](generics.md)`. These are sibling links within the stories folder, living, not permalinks.

### 4.4 The spec's pointer back

A spec section whose *why* is non-trivial ends with a pointer into the story that tells it:

```markdown
> **Story:** [`stories/<topic>.md`](../stories/<topic>.md) — the <topic> story <short description of what it explains>.
```

The href is the story file with **no anchor** — the story is one continuous narrative, so the pointer lands the reader at the top and the description tells them what the story develops. Put the pointer where the curious reader is — at the section whose rule it explains — and keep its description specific enough that a reader knows what thread to follow once they arrive.

---

## 5. Updating a Story When the Spec Changes

This is the discipline that makes the folder a *history* rather than a stale snapshot.

**Append, don't overwrite.** When the design changes, the old reasoning did not become false — it became *the previous turn in the story*. So when the spec moves, add to the narrative: extend it with a new passage that names the cause and what it forced — *"The shift to X meant the old Y no longer held, so we…"* — and pin its spec references to the new commit (§4.2). The discarded path stays on the page as the record of why the design used to be one way and is now another; that causal trail is often the most illuminating thing in the file, and rewriting it away destroys it.

**Consolidate dead threads, sparingly.** Appending forever would bury the present under history. So a passage *may* be rewritten or folded down — but only when its narrative has become pure dead weight: it no longer illuminates the present design *and* is not interesting as history. That is a high bar. The default is to append; consolidation is the rare exception, not routine cleanup, and when in doubt you keep the history.

The contrast to hold in mind: the **spec** is rewritten to the present on every change — it states only what is true now. The **story** accumulates — it states how what is true now came to be. They have opposite update rules on purpose.

---

## 6. Prose Style

Stories docs share the spec guide's [§6 prose rules](writing-spec-docs.md): `` `backticks` `` for identifiers, `**bold**` for a term's first definition or a key claim, italics only for document names and semantic categories. But the register is looser than the spec's, because this is a story, not a definition:

- **First person and judgement are welcome.** "We rejected X because…", "the elegant framing oversells this" — a stories doc is where opinion and honest doubt belong. The spec stays neutral; the story need not.
- **Long-form prose is the point.** The spec keeps sentences short and clipped; a story can breathe, run longer sentences, and carry a narrative thread from the first paragraph to the last. Readability as a continuous history matters more than terse rule-statement here.
- **Be honest about costs.** The reason anyone will trust the record later is that it admits what the decision gave up.

---

## 7. What Does Not Belong Here

- **Normative rules, syntax, type rules, MUST/MUST NOT.** Those are the spec's job. A stories doc references them; it does not restate or amend them.
- **Language-wide philosophy.** The cross-cutting thesis (staging, captured intent, strict-rules-buy-fast-codegen) lives once in [`stories/foundations.md`](../stories/foundations.md). A topic stories doc links to it.
- **Anything a reader needs in order to *use* the language.** If they need it to write correct Zane, it is normative and belongs in `spec/`. The stories folder is for understanding *how and why*, never for *how-to*.

---

## 8. Adding a New Stories Document

1. Create `stories/<topic>.md`, matching the spec file name exactly (§2.1).
2. Follow the shape in §2 — title, optional `> **See also:**`, then one continuous narrative with no separators or per-decision headings.
3. Write the narrative in the order the thinking moved (§3), one episode flowing into the next.
4. Add a `> **Story:**` pointer from each non-trivially-justified section of the matching spec doc (§4.4), and pin in-prose spec references by permalink (§4.2).
5. Add a row to the stories table in [`README.md`](../README.md).
