# Writing Zane Rationale Docs

This guide describes the conventions for writing and maintaining **rationale documents** in [`rationale/`](../rationale/). Follow it when adding a new rationale document or extending an existing one. Its sibling, [`writing-spec-docs.md`](writing-spec-docs.md), governs the normative spec documents in [`spec/`](../spec/).

---

## 1. What a Rationale Doc Is

The spec answers *what the language does*. A rationale doc answers *why it does that*. The two are kept in separate folders because they have different jobs, different audiences, and — crucially — different correctness criteria.

- **A spec doc must be current.** It is the definition; if it is out of date it is wrong.
- **A rationale doc must be honest to its moment.** It is a design journal — a record of the thinking at the time a decision was made. An old entry that describes old thinking is not stale; it is history.

That difference is what makes the split safe. A rationale doc is **not normative**: nothing in it overrides a spec rule. If a rationale entry and the spec ever disagree, the spec wins and the rationale entry is a historical artifact to be superseded (§5), not corrected in place.

Why a separate folder instead of a rationale table at the end of each spec doc:

- **`A table flattens.`** Genuinely complex reasoning — the alternatives weighed, the tension resolved, the cost accepted — does not fit a one-sentence cell. Forcing it there serves the trivial decisions and starves the deep ones.
- **`Different cadence.`** The spec is terse and stable. Rationale is exploratory and grows. Separating them lets each move at its own pace without bloating the other.
- **`The why is the captured thought.`** Zane treats code as captured intent; a rationale doc is that same intent captured one level up, at the design.

---

## 2. Required Document Shape

A rationale document is looser than a spec document on purpose — a journal needs room to narrate. But it is not formless. Every rationale document has this shape:

```
# Rationale: <Topic>

One or two sentences: this is the design journal for spec/<topic>.md.
A short paragraph stating the append-only / dated / non-normative rule.

> **See also:** [`writing-rationale-docs.md`](../contributing/writing-rationale-docs.md) for this guide. [`spec/<topic>.md`](../spec/<topic>.md) for the spec doc these entries justify.

---

## <Decision name>
**Spec:** [`<topic>.md`](../spec/<topic>.md) §N   ·   **Settled:** <YYYY-MM>

<entry body — see §4>

---

## <Decision name>
...
```

### 2.1 File name and location

A rationale document **mirrors its spec document's file name**: the rationale for [`spec/generics.md`](../spec/generics.md) lives at [`rationale/generics.md`](../rationale/generics.md). One rationale file per spec file. This makes discovery mechanical — same name, sibling folder — and gives the anti-drift rule a simple form: *editing `spec/X.md`? check `rationale/X.md`.*

Do not create one file per decision (too many stubs) or one journal for the whole language (unnavigable). The cross-cutting, language-wide *why* belongs in [`spec/foundations.md`](../spec/foundations.md), not here; a topic rationale doc cross-references it rather than restating it.

### 2.2 Title and lead-in

```
# Rationale: <Topic>
```

`Rationale:` followed by the same topic noun phrase the spec doc uses (`# Rationale: Generics and Type Parameters` for `generics.md`'s `# Zane Generics and Type Parameters`). Follow with one or two sentences of plain-English lead-in, then a short paragraph that states the append-only, dated, non-normative rule so a first-time reader knows what kind of document they are in.

### 2.3 See also block

Immediately after the lead-in, a `> **See also:**` block linking at least this guide and the spec document the entries justify. Same style as the spec guide: `>` blockquote, bold `**See also:**`, filenames as link text, entries separated by `. `.

### 2.4 Separators

A bare `---` after the lead-in/See-also block, and one between every entry. Never inside an entry.

---

## 3. Entries Are Dated and Append-Only

This is the rule that makes a rationale doc a journal rather than a second spec to keep in sync.

- Each entry records a `**Settled:**` date (`YYYY-MM` is enough).
- **When a decision changes, add a new entry — do not rewrite the old one.** The new entry gets its own date and opens by noting what it supersedes; the old entry stays, marked superseded (§5). The file reads forward in time.
- The current reasoning is therefore the *latest* entry on a given decision. A reader who wants only the present view reads the most recent dated entry; a reader who wants the story reads the chain.

The cost of append-only is real: entries accumulate, and the current view is at the bottom, not the top. We accept it, because a design journal whose past gets overwritten is not a journal — and because it is exactly what removes the drift pressure that a parallel normative doc would create.

---

## 4. Entry Format

An entry is mostly prose. Only two things are required; everything else is a recommended rhythm, used as the decision needs it.

### 4.1 Required

1. **A decision name** as the `## ` heading — a short noun phrase naming the decision, not a question.
2. **A spec back-link** on the line under the heading: **Spec:** [`<topic>.md`](../spec/<topic>.md) §N. This is the anti-drift anchor — the one piece of discipline that keeps the journal tied to the rules it explains. Add `· **Settled:** <YYYY-MM>` after it.

### 4.2 Recommended rhythm

Most entries read well in four movements. None are mandatory headings — write them as flowing prose, and skip any that a given decision does not need:

1. **The fork.** What question forced a decision here *at all*. If there was no real alternative, the decision probably is not worth an entry.
2. **Alternatives weighed.** Each road not taken, and why it lost. Mark a discarded design with `❌` at the start of its paragraph or line. This is the highest-value part — it is precisely what a spec cannot hold.
3. **The resolution.** The choice and its reasoning chain, as long as it needs to be. A page is fine; so is a sentence.
4. **Costs / deferred.** What this decision makes *worse*, what it defers, what open question it leaves. Near-mandatory in spirit: it is the part the spec can never carry and the part most worth preserving. Lead the line with **Costs / deferred:** or fold it into closing prose.

### 4.3 Length

As long as the decision warrants, as short as it allows. A trivial decision is two sentences. A foundational one is two pages. Letting the length flex with the decision is the entire reason this folder exists instead of a table.

---

## 5. Superseding an Entry

When a later decision overturns an earlier one:

- Keep the old entry. Add `· **Superseded:** <YYYY-MM> — see "<new entry name>"` to its metadata line.
- Write a new entry whose opening sentence names what it replaces and why the earlier reasoning no longer holds.
- Update the spec and its `> **Rationale:**` cross-reference (§6) to point at the live state.

Never delete the reasoning that led to a now-abandoned design. The discarded path is often the most instructive part of the record.

---

## 6. Cross-Linking With the Spec

The two folders are joined by links in both directions:

- **Spec → rationale.** A spec section whose *why* is non-trivial ends with a pointer:

  ```markdown
  > **Rationale:** [`rationale/<topic>.md`](../rationale/<topic>.md) — "<decision name>".
  ```

  This replaces the old per-document Design Rationale table. Put it where the curious reader is — at the section whose rule it explains — not only at the foot of the file.

- **Rationale → spec.** Every entry's required `**Spec:**` back-link (§4.1) closes the loop.

When section numbers in the spec change, update the `**Spec:**` anchors in the matching rationale file in the same change — the same discipline the spec guide already requires for its own cross-references.

---

## 7. Prose Style

Rationale docs share the spec guide's [§6 prose rules](writing-spec-docs.md): short sentences, one idea each, active voice, `` `backticks` `` for identifiers, `**bold**` for a term's first definition or a key claim, italics only for document names and semantic categories.

Two differences in register, because this is a journal, not a definition:

- **First person and judgement are allowed.** "We rejected X because…", "the elegant framing oversells this" — a rationale doc is where opinion and honest doubt belong. The spec stays neutral; the journal need not.
- **Be honest about costs.** An entry that only lists the upsides of a decision is incomplete. The `❌` alternatives and the costs line are not garnish — they are the reason anyone will trust the record later.

---

## 8. What Does Not Belong Here

- **Normative rules, syntax, type rules, MUST/MUST NOT.** Those are the spec's job. A rationale doc references them; it does not restate or amend them.
- **Language-wide philosophy.** The cross-cutting thesis (staging, captured intent, strict-rules-buy-fast-codegen) lives once in [`spec/foundations.md`](../spec/foundations.md). A topic rationale doc links to it.
- **Anything a reader needs in order to *use* the language.** If they need it to write correct Zane, it is normative and belongs in `spec/`. The rationale folder is for understanding *why*, never for *how*.

---

## 9. Adding a New Rationale Document

1. Create `rationale/<topic>.md`, matching the spec file name exactly (§2.1).
2. Follow the shape in §2.
3. Write one entry per real decision, ordered by conceptual importance, not by spec section order.
4. Add a `> **Rationale:**` pointer from each non-trivially-justified section of the matching spec doc (§6).
5. Add a row to the rationale table in [`README.md`](../README.md).
