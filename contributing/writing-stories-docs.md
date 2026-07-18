# Writing Zane Stories Docs

This guide describes the conventions for writing and maintaining **stories documents** in [`stories/`](../stories/). Follow it when adding a new stories document or extending an existing one. Its sibling, [`writing-spec-docs.md`](writing-spec-docs.md), governs the normative spec documents in [`spec/`](../spec/).

---

## 1. What a Stories Doc Is

The spec answers *what the language does*. A stories doc answers *how it came to do that* — the reasoning, told as it actually unfolded. The two are kept in separate folders because they have different jobs and different audiences.

A stories doc is **not normative**: nothing in it overrides a spec rule. The spec is the source of truth; a stories doc explains the thinking behind it. If the two ever disagree, the spec wins and the story is what gets fixed.

The reasoning lives in its own folder, rather than in a table at the foot of each spec doc, for one reason: **a table flattens.** Genuinely complex reasoning — the alternatives weighed, the tension resolved, the cost accepted — does not fit a one-sentence cell. A separate file lets the reasoning have the room it needs, and lets the spec stay terse.

**A stories doc reads like a history, not like a spec.** A spec entry is a rule. A story is the account of how that rule came to be — the situation that forced a choice, the roads not taken, the reasoning that settled it, and the cost it carries. Crucially, the unit is **not the individual decision.** Decisions do not exist as tidy, separable atoms; they crystallize out of a line of reasoning, in response to a pressure, often several at once. Writing one short justification per spec section pretends otherwise — it pretends the author was thinking decision-by-decision when they were really following a thread. A stories doc follows the thread instead.

---

## 2. Document Shape

A stories document is a title, a `> **See also:**` pointer to the spec, and a small number of **chapters** in the order the thinking moved through them. Each chapter is a *theme* — a phase of the design in which one pressure was met and resolved — and it reads as flowing prose that runs from one chapter into the next. Chapters are marked by `## ` headings and **nothing else**: there are no `---` separators between them, and the prose carries the reader across each boundary on the thread of cause and consequence.

```
# Stories: <Topic>

> **See also:** [`spec/<topic>.md`](../spec/<topic>.md) — the rules these chapters explain.

## <Chapter name>

<the chapter — flowing prose. Opens on the situation that forced a choice,
tells what was decided in response (usually several related things at once),
the roads not taken, and the cost. Links to specific spec points by permalink
when it discusses them (§4), and to sibling chapters by anchor (§4.3).>

## <Chapter name>

...
```

Do **not** open the file with a paragraph explaining what stories documents are or how they work — that is this guide's job, not the file's. A reader who opens `stories/generics.md` wants the generics story, not a preamble. Start with the title, the `> **See also:**` line pointing at the matching spec doc, and then the first chapter.

### 2.1 File name and location

A stories document **mirrors its spec document's file name**: the story behind [`spec/generics.md`](../spec/generics.md) lives at [`stories/generics.md`](../stories/generics.md). One stories file per spec file. This makes discovery mechanical — same name, sibling folder — and gives the anti-drift habit a simple form: *editing `spec/X.md`? check `stories/X.md`.*

Do not create one file per chapter (too fragmentary) or one file for the whole language (unnavigable). The cross-cutting, language-wide *why* belongs in [`stories/foundations.md`](../stories/foundations.md); a topic stories doc cross-references it rather than restating it.

### 2.2 A chapter is a theme, not a decision

A chapter is a **phase of the design** — an episode in which one pressure was met and resolved — and it almost always settles **several related decisions at once**, because that is how they were actually made. It is named by a `## ` heading: a short noun phrase that names the *theme* ("URL identity and the two-file manifest", "No turbofish: passing types as values"), not a question and not a single spec rule.

This is the rule to hold on to: **do not write one chapter per spec decision.** A spec section is a fine-grained rule; a chapter is the coarser line of reasoning that produced a cluster of such rules. Group decisions that were forced by the same pressure into one chapter and let them play out as a single story; conversely, one large decision may run across several chapters if the thinking really arrived in stages. Let the chapter boundaries fall where the *reasoning* has joints — typically a handful of chapters per file — not where the spec has section numbers. There is no required template below the heading, no labelled parts, no metadata ritual. Everything under the heading is the story.

A chapter is **not pinned to a spec section.** It is free to range across whatever the episode touched. It *should* link to specific spec rules where it discusses them (§4), but those links serve the reader; they are not a structural anchor, and a chapter is never reshaped just to line up one-to-one with a `§N`.

---

## 3. Writing the Chapter

A good chapter tends to move through four things — but as *prose*, never as labelled sections:

- **The situation.** What pressure forced a choice here at all — the cause. A chapter that records no real pressure is probably not a chapter; it is a rule, and the spec already has it.
- **The roads not taken.** The alternatives, and why each lost. This is the highest-value part — it is precisely what a spec cannot hold — so do not skip it, but tell it in sentences ("We could have done X, the way C++ does, but…"), not as a bulleted list of rejects.
- **The resolution.** What was decided, and the reasoning that settled it.
- **The cost.** What the decision makes worse, what it defers, what it leaves open. Where a decision carries a real cost, naming it is the most valuable thing a chapter can do — it is the part later readers will most want to have been told, and a chapter that lists only upsides usually means the cost went unexamined, not that there wasn't one. But this is a strong recommendation, not a requirement: do not invent or inflate a downside just to have one. If, after genuinely looking, the honest account is that the decision cost little or nothing, say that plainly rather than manufacturing a drawback the spec does not bear out.

Frame a cost as an *inherent* property of the design, never as a migration burden. The toolchain versions itself (see [`dependencies.md`](../spec/dependencies.md) §14), so there is no existing body of code that a change must avoid breaking — never state a decision's cost as porting or breaking old code. Name the inherent cost instead: what the language now makes harder or impossible.

Let the length flex with the episode: a minor turn is a paragraph, a foundational one runs to pages.

**Order chapters by the path the thinking took**, not by spec section order and not by tidy importance. A chapter usually opens where the previous one left off — one choice creates the problem the next one solves — so the natural order is causal and roughly chronological, and because there are no separators, the opening sentence of each chapter should carry the reader across the seam ("With identity settled, the next question is…", "Shipping prebuilt objects raises a problem the moment…"). "When" here means *relative to the other decisions* ("once we had settled X, the next pressure was Y"), not a calendar date; record a real date or commit only if it genuinely matters. Recording the discarded attempts in the order they were tried is often clearer than a tidy after-the-fact summary: it shows *why* the final design has the shape it does, and it stops a future reader from re-walking roads already known to be dead.

---

## 4. Linking to the Spec

A stories doc is joined to the spec, and its chapters are joined to each other. Two kinds of link do two different jobs, and they are written differently.

### 4.1 The companion pointer (living)

The top-of-file `> **See also:**` line, and the spec's own `> **Story:**` pointers back (§4.4), are **navigational**: they say "here is the companion document." A reader following one wants the *current* doc, so these stay ordinary repo-relative links (`../spec/<topic>.md`, `../stories/<topic>.md`). They are expected to track the files as they move.

### 4.2 In-prose spec references (point-in-time → permalink)

When a chapter says *"the spec says X"* — when it quotes or leans on a specific rule as it stood when the chapter was written — that is a **point-in-time claim**, and it must be a **permalink** pinned to a commit, not a moving relative link:

```markdown
[`lexical.md` §5](https://github.com/zane-lang/spec/blob/<commit-sha>/spec/lexical.md#5-how-casing-disambiguates-the-grammar)
```

The reason is the update model (§5). A story accumulates; a chapter written today describes the spec *as it is today*. A relative link would silently re-point as the spec changes, so an old chapter would end up "citing" a rule that no longer says what the chapter claims — invisible drift, exactly the failure the anti-drift discipline exists to prevent. Pinning the link to the commit freezes the citation to the spec the chapter was actually written against, so a later reader can always see what was true *then* and judge the reasoning on its own terms.

Use a full GitHub blob permalink with a commit SHA (`/blob/<sha>/...`), not a branch or tag name — branches and tags move, which defeats the point. Pin to the latest commit that holds the spec text the chapter describes; for a chapter written alongside a spec change, that is the commit that change lands in. The visible link text should still name the file and section (`` `lexical.md` §5``) so the citation reads naturally; the SHA rides in the URL.

Linking like this is **recommended wherever a chapter discusses a specific rule**, but it is not mandatory and a chapter is never bent to accommodate one. A chapter with no spec link is fine; the companion pointer (§4.1) already connects the reader to the doc.

### 4.3 Between chapters

Chapters may — and often should — link to **each other**. A design decision rarely stands alone: one choice creates the problem the next one solves, and that lineage is itself part of the story. So let a chapter open where another left off — *"Once we had decided a call carries no `<>` list, a caller had no way to name a type that inference couldn't reach, so we needed…"* — and name the chapter it builds on. Link by the chapter's heading **anchor** — `[the parameter model](#the-parameter-model)` — so the chain is clickable; let the visible text stay a natural phrase. These are sibling links *within the stories folder*, so they are ordinary relative anchors (living), not permalinks. A reference to a chapter in a *different* stories file uses a sibling-relative path plus anchor: `[the generics story](generics.md#types-are-templated-functions)`.

### 4.4 The spec's pointer back

A spec section whose *why* is non-trivial ends with a pointer into the chapter that tells it:

```markdown
> **Story:** [`stories/<topic>.md`](../stories/<topic>.md#<anchor>) — "<chapter name>".
```

The href ends in the chapter's heading **anchor** so the link scrolls straight there, and the quoted text is the heading itself. Put it where the curious reader is — at the section whose rule it explains. Because a chapter is a theme rather than a single decision, several spec sections may point at the same chapter — that is expected. When a chapter heading changes, its anchor changes too, so fix every inbound `> **Story:**` pointer in the same change.

---

## 5. Updating a Story When the Spec Changes

This is the discipline that makes the folder a *history* rather than a stale snapshot.

**Append, don't overwrite.** When the design changes, the old reasoning did not become false — it became *the previous chapter*. So when the spec moves, add to the story: open a new chapter (or extend the relevant one) that names the cause and what it forced — *"The shift to X meant the old Y no longer held, so we…"* — and pin its spec references to the new commit (§4.2). The discarded path stays on the page as the record of why the design used to be one way and is now another; that causal trail is often the most illuminating thing in the file, and rewriting it away destroys it.

**Consolidate dead threads, sparingly.** Appending forever would bury the present under history. So a chapter *may* be rewritten or folded down — but only when its narrative has become pure dead weight: it no longer illuminates the present design *and* is not interesting as history. That is a high bar. The default is to append; consolidation is the rare exception, not routine cleanup, and when in doubt you keep the history.

The contrast to hold in mind: the **spec** is rewritten to the present on every change — it states only what is true now. The **story** accumulates — it states how what is true now came to be. They have opposite update rules on purpose.

---

## 6. Prose Style

Stories docs share the spec guide's [§6 prose rules](writing-spec-docs.md): `` `backticks` `` for identifiers, `**bold**` for a term's first definition or a key claim, italics only for document names and semantic categories. But the register is looser than the spec's, because this is a story, not a definition:

- **The voice is the first-person plural "we" — always.** A stories doc is written as the collective voice of the design: *"we rejected X because…"*, *"we had already committed to…"*. Use **"we" even for a decision one person made alone** — the story speaks for the design, not the individual — and **never the singular "I."** This is a firm rule, not a preference: a story that slips into "I" is fixed to "we." Judgement and honest doubt belong in this voice too (*"the elegant framing oversells this"*): the spec stays neutral, the story need not — but it is always neutral about *who*, because it is always "we."
- **Past tense for the reasoning, present tense for the standing design.** Recount how a decision was reached in the past — *"we rejected that"*, *"we had already committed"* — because that is history. State what the design *is* in the present — *"an `enum` is a closed set of peers"*, *"a call never carries a `<>` list"* — because that is still true. A chapter typically opens in the present to frame the situation, then moves into the past to tell the deliberation.
- **Long-form prose is the point.** The spec keeps sentences short and clipped; a story can breathe, run longer sentences, and carry a narrative thread within and across chapters. Readability as a history matters more than terse rule-statement here.
- **Be honest about costs.** The reason anyone will trust the record later is that it admits what the decision gave up. Honesty cuts both ways, though: where a decision genuinely cost little, saying so is more trustworthy than inventing a drawback to look balanced (§3).

---

## 7. What Does Not Belong Here

- **Normative rules, syntax, type rules, MUST/MUST NOT.** Those are the spec's job. A stories doc references them; it does not restate or amend them.
- **Language-wide philosophy.** The cross-cutting thesis (staging, captured intent, strict-rules-buy-fast-codegen) lives once in [`stories/foundations.md`](../stories/foundations.md). A topic stories doc links to it.
- **Anything a reader needs in order to *use* the language.** If they need it to write correct Zane, it is normative and belongs in `spec/`. The stories folder is for understanding *how and why*, never for *how-to*.

---

## 8. Adding a New Stories Document

1. Create `stories/<topic>.md`, matching the spec file name exactly (§2.1).
2. Follow the shape in §2 — title, the `> **See also:**` pointer, then a handful of thematic chapters with `## ` headings and no `---` separators.
3. Write the chapters in the order the thinking moved (§3), grouping related decisions rather than splitting one chapter per spec rule, in the first-person-plural "we" voice throughout (§6).
4. Add a `> **Story:**` pointer from each non-trivially-justified section of the matching spec doc (§4.4), and pin in-prose spec references by permalink (§4.2).
5. Add a row to the stories table in [`README.md`](../README.md).
