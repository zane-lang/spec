# Writing Zane Rationale Docs

This guide describes the conventions for writing and maintaining **rationale documents** in [`rationale/`](../rationale/). Follow it when adding a new rationale document or extending an existing one. Its sibling, [`writing-spec-docs.md`](writing-spec-docs.md), governs the normative spec documents in [`spec/`](../spec/).

---

## 1. What a Rationale Doc Is

The spec answers *what the language does*. A rationale doc answers *why it does that*. The two are kept in separate folders because they have different jobs and different audiences.

A rationale doc is **not normative**: nothing in it overrides a spec rule. The spec is the source of truth; a rationale doc explains the thinking behind it. If the two ever disagree, the spec wins and the rationale is what gets fixed.

Rationale lives in its own folder, rather than in a table at the foot of each spec doc, for one reason: **a table flattens.** Genuinely complex reasoning — the alternatives weighed, the tension resolved, the cost accepted — does not fit a one-sentence cell. A separate file lets the reasoning have the room it needs, and lets the spec stay terse.

**Rationale docs read like stories, not like specs.** A spec entry is a rule; a rationale entry is the account of how that rule came to be — the fork that forced a choice, the roads not taken, the reasoning that settled it, and the cost it carries. Write it as flowing prose, the way you would explain the decision to a colleague. There is no required template, no section scaffolding, no metadata ritual. The only fixed points are a heading per decision and a link back to the spec (§2).

---

## 2. Document Shape

A rationale document is a title, an optional pointer to the spec, and a series of stories — one per design decision — separated by `---`.

```
# Rationale: <Topic>

> **See also:** [`spec/<topic>.md`](../spec/<topic>.md) for the rules these stories explain.

---

## <Decision name>
**Spec:** [`<topic>.md`](../spec/<topic>.md) §N

<the story — flowing prose, as long or short as the decision warrants>

---

## <Decision name>
...
```

Do **not** open the file with a paragraph explaining what rationale documents are or how they work — that is this guide's job, not the file's. A reader who opens `rationale/generics.md` wants the generics stories, not a preamble. Start with the title, an optional `> **See also:**` line pointing at the matching spec doc, and then the first story.

### 2.1 File name and location

A rationale document **mirrors its spec document's file name**: the rationale for [`spec/generics.md`](../spec/generics.md) lives at [`rationale/generics.md`](../rationale/generics.md). One rationale file per spec file. This makes discovery mechanical — same name, sibling folder — and gives the anti-drift habit a simple form: *editing `spec/X.md`? check `rationale/X.md`.*

Do not create one file per decision (too many stubs) or one file for the whole language (unnavigable). The cross-cutting, language-wide *why* belongs in [`rationale/foundations.md`](../rationale/foundations.md); a topic rationale doc cross-references it rather than restating it.

### 2.2 What an entry needs

Each story has exactly two fixed parts:

1. **A decision name** as the `## ` heading — a short noun phrase naming the decision, not a question.
2. **A spec back-link** on the line directly under the heading: `**Spec:** [`<topic>.md`](../spec/<topic>.md) §N`. This is the anti-drift anchor: it ties the story to the rule it explains, and it is what a reader (or a reviewer) follows to check the two still agree.

Everything below that line is the story. No sub-headings, no labelled parts, no metadata beyond the spec link.

---

## 3. Writing the Story

A good rationale story tends to move through four things — but as *prose*, never as labelled sections:

- **The fork.** What question forced a decision here at all. If there was no real alternative, the decision probably is not worth its own story.
- **The roads not taken.** The alternatives, and why each lost. This is the highest-value part — it is precisely what a spec cannot hold — so do not skip it, but tell it in sentences ("We could have done X, the way C++ does, but…"), not as a bulleted list of rejects.
- **The resolution.** The choice and the reasoning that settled it.
- **The cost.** What the decision makes worse, what it defers, what it leaves open. An honest story always reaches this; a story that only lists upsides is incomplete, and the cost is the part later readers will most want to have been told.

Let the length flex with the decision: a minor choice is a paragraph, a foundational one runs to pages. Order the stories by conceptual importance, not by spec section order.

Those four movements are a tendency, not a cage. A story can instead retrace the **actual path** the thinking took — the first idea, why it broke, the next one, the dead end after that, and the approach that finally held. Recording the discarded attempts in the order they were tried is often clearer than a tidy after-the-fact summary: it shows *why* the final design has the shape it does, and it stops a future reader from re-walking roads already known to be dead. Write whichever shape tells the truth of the decision better — a clean fork-and-resolution where the decision was clean, a winding "we tried X, then Y, then realised Z" where it actually wound.

When the design changes, **rewrite the story to match.** The spec is the record of what is true; the rationale is the current explanation of why. Keep it honest to the present design rather than accumulating a changelog.

---

## 4. Cross-Linking

A rationale doc is joined to the spec, and its stories are joined to each other.

### 4.1 With the spec

The two folders are linked in both directions:

- **Spec → rationale.** A spec section whose *why* is non-trivial ends with a pointer:

  ```markdown
  > **Rationale:** [`rationale/<topic>.md`](../rationale/<topic>.md) — "<decision name>".
  ```

  The quoted text is the story's exact heading, so the link lands on the right decision. Put it where the curious reader is — at the section whose rule it explains — not only at the foot of the file.

- **Rationale → spec.** Every story's `**Spec:**` back-link (§2.2) closes the loop.

When section numbers or story headings change, update both sides in the same change — the same discipline the spec guide already requires for its own cross-references.

### 4.2 Between stories

Stories may — and often should — link to **each other**, not just to the spec. A design decision rarely stands alone: one choice creates the problem the next one solves, and that lineage is itself part of the why. So let a story open where another left off — *"Once we had decided a call carries no `<>` list, a caller had no way to name a type that inference couldn't reach, so we needed…"* — and name the story it builds on. A decision that exists *only* because of an earlier one should make that debt explicit; a reader who can follow the chain understands the design as a sequence of forced moves rather than a pile of independent rules, and that chain is frequently the most illuminating thing in the file. Reference another story by its heading, the way the spec pointers do, and don't be shy about narrating the connective reasoning across decisions rather than quarantining each one.

---

## 5. Prose Style

Rationale docs share the spec guide's [§6 prose rules](writing-spec-docs.md): `` `backticks` `` for identifiers, `**bold**` for a term's first definition or a key claim, italics only for document names and semantic categories. But the register is looser than the spec's, because this is a story, not a definition:

- **First person and judgement are welcome.** "We rejected X because…", "the elegant framing oversells this" — a rationale doc is where opinion and honest doubt belong. The spec stays neutral; the story need not.
- **Long-form prose is the point.** The spec keeps sentences short and clipped; a rationale story can breathe, run longer sentences, and carry a narrative thread. Readability as a story matters more than terse rule-statement here.
- **Be honest about costs.** The reason anyone will trust the record later is that it admits what the decision gave up.

---

## 6. What Does Not Belong Here

- **Normative rules, syntax, type rules, MUST/MUST NOT.** Those are the spec's job. A rationale doc references them; it does not restate or amend them.
- **Language-wide philosophy.** The cross-cutting thesis (staging, captured intent, strict-rules-buy-fast-codegen) lives once in [`rationale/foundations.md`](../rationale/foundations.md). A topic rationale doc links to it.
- **Anything a reader needs in order to *use* the language.** If they need it to write correct Zane, it is normative and belongs in `spec/`. The rationale folder is for understanding *why*, never for *how*.

---

## 7. Adding a New Rationale Document

1. Create `rationale/<topic>.md`, matching the spec file name exactly (§2.1).
2. Follow the shape in §2 — title, optional `> **See also:**`, then the stories.
3. Write one story per real decision, ordered by conceptual importance.
4. Add a `> **Rationale:**` pointer from each non-trivially-justified section of the matching spec doc (§4).
5. Add a row to the rationale table in [`README.md`](../README.md).
