# Writing Zane Spec Docs

This guide describes the conventions for writing and maintaining spec documents in this repository. Follow it when adding a new document, extending an existing one, or editing any section.

Spec documents state *what the language does*. The *why* — the reasoning, the alternatives, the accepted costs — lives in a separate place: see [`writing-stories-docs.md`](writing-stories-docs.md) for the stories folder and its conventions. Keeping the two apart is deliberate; this guide governs only the normative spec.

---

## 1. Document Inventory

Every spec document lives in [`spec/`](../spec/), and each file covers one topic area. Each topic has a single *canonical* home document. Its *why* lives in a sibling stories doc at `stories/<same-name>.md`.

If you need to mention a concept that is canonically specified elsewhere, keep the mention brief and add a cross-reference rather than duplicating rules.

When in doubt about where a piece of content belongs, ask: does it describe *what the language does* (topic doc in `spec/`), *how to write it* (`syntax.md`), *what the spec calls it* (`glossary.md`), or *how it came to be decided that way* (`stories/`)? If a reader needs it to write correct Zane, it is normative and belongs in `spec/`. If it only helps them understand a choice, it belongs in `stories/`.

See [`README.md`](../README.md) for the canonical index of every spec document and its purpose.

---

## 2. Required Document Shape

Every **topic spec document** must follow this top-to-bottom shape:

```
# Zane <Name>

One or two sentences describing what this document covers.

> **See also:** cross-references to related documents (optional but strongly encouraged)

---

## 1. Overview

Short orienting paragraph — what the feature is and why it exists.
Bullet list of the 2–5 core ideas.

---

## 2. <First Topic>

### 2.1 <Subtopic>

...
A section whose reasoning is non-trivial ends with a
> **Story:** pointer into stories/<name>.md   (see §3.4)

---

## N. Language Comparisons    ← only if comparisons apply to this document

Comparison tables and per-language breakdowns (see §4).

---

## N+1. Summary    ← only in topic docs with many rules

Two-column summary table (see §3.5).
```

A topic document **does not** contain a Design Rationale section. The reasoning lives in `stories/` (§3.4). Spec sections carry only the brief, in-place justification that a reader needs to *understand the rule as stated*; the developed why — forks, discarded alternatives, accepted costs — goes in the matching stories doc and is reached by a `> **Story:**` pointer.

### 2.1 Title line

```
# Zane <Name>
```

Always `Zane` followed by a descriptive noun phrase. Examples:
- `# Zane Memory Model`
- `# Zane Error Handling`
- `# Zane Effect Model`

### 2.2 Lead-in prose

One or two sentences immediately after the title, before the first `---`. Describes in plain English what the document covers. No heading — just body text.

```markdown
This document specifies Zane's memory model: how objects are owned and destroyed, how memory is laid out and allocated, and how non-owning references are safely tracked through the anchor system.
```

### 2.3 See also block

Immediately after the lead-in, before the first `---`, if cross-references are useful:

```markdown
> **See also:** [`types.md`](types.md) for class/struct declarations. [`effects.md`](effects.md) for the effect model.
```

Use `>` blockquote with bold `**See also:**`. Link text is the filename. Description is a short phrase. Separate entries with `. ` (period space). Spec documents always link to siblings inside `spec/` using a bare filename — for example, `types.md` rather than `spec/types.md` — since they live in the same directory. A link into the stories folder uses a relative path: `../stories/types.md`.

### 2.4 Section separators (`---`)

Use a bare `---` line:
- Once after the lead-in (and optional “See also” block), before `## 1. Overview`.
- Between every top-level section (`## N.`).

Never put `---` between subsections (`### N.M`).

### 2.5 Section numbering

Top-level sections are numbered `## 1.`, `## 2.`, etc. Subsections are `### 1.1`, `### 1.2`, etc.

Rules:
- Do not skip numbers.
- Do not restart numbering.
- In topic documents, the Overview is always `## 1.`.

### 2.6 Structural exceptions

`syntax.md` is a canonical reference document, not a topic narrative. It still uses the same title style, lead-in prose, `---` separators, and numbered subsections, but it may start directly with grammar categories such as Declarations, Types, and Calls instead of a dedicated `## 1. Overview` section.

`glossary.md` is also a reference document rather than a topic narrative. It should still use the same title style, lead-in prose, `---` separators, and numbered groups / per-term subsections, but it does **not** need a topic-style Overview / Language Comparisons layout. Instead, it should organize terms into clear groups and, for each term, record:

- the preferred term
- a short meaning
- why that name was chosen
- the canonical home document for the full rule

---

## 3. Section Types

### 3.1 Overview

Every topic document begins with `## 1. Overview`. It contains:

- A short paragraph explaining what the feature is.
- A bullet list of 2–5 core ideas, each formatted as **`keyword`** with a brief explanation.

```markdown
## 1. Overview

Zane uses a **structural effect model** with a single user-facing effect modifier: `mut`.

- **Single ownership.** Every heap object has exactly one owner at all times.
- **Anchor-based refs.** An `&` points through a stable anchor, never directly at an object.
```

The Overview is orientation, not rationale: it says what the feature *is*, not why it was chosen over the alternatives. If one of the core ideas is non-obvious, name it here in one line and point to the stories doc for the argument.

### 3.2 Core topic sections

Numbered `## 2.` onward. Write prose that explains *what the language does*, grounded in the rules, with enough *why* that the rule reads as sensible rather than arbitrary. The *developed* why — the forks, the rejected designs, the costs — does not go here; it goes in the stories doc (§3.4). Include code examples in every subsection that has a non-obvious behaviour.

Do not include syntax grammar forms in topic docs. Those go in `syntax.md`. Cross-reference with:

```markdown
> **See also:** [`syntax.md`](syntax.md) §1 and §3 for declaration forms.
```

### 3.3 Language Comparisons section

Include this section only when Zane's design is meaningfully different from mainstream alternatives. See §4 for format.

### 3.4 Story cross-references

A spec document records its design reasoning **not** as an in-document table but in a sibling stories doc at `stories/<same-name>.md`, governed by [`writing-stories-docs.md`](writing-stories-docs.md).

At the end of any section whose reasoning is non-trivial, add a pointer:

```markdown
> **Story:** [`stories/memory.md`](../stories/memory.md) — the memory story explains why ownership is single by default.
```

Rules:
- Put the pointer at the section whose rule it explains, where a curious reader actually is — not only at the foot of the file.
- The href is the story file with no anchor — a stories doc is one continuous narrative, so the pointer lands at the top and the description tells the reader what thread it develops.
- A section whose rule is self-evident needs no pointer. Do not manufacture story material for trivial decisions.
- When you add or change a rule, extend its story in the same change (see §8). The story is *appended to*, not rewritten — see [`writing-stories-docs.md`](writing-stories-docs.md) §5.

The `> **Story:**` pointer is a *living* link into the current story; in the other direction, when the story quotes a specific spec rule it pins that reference to a commit with a permalink (see [`writing-stories-docs.md`](writing-stories-docs.md) §4). This replaces the former per-document Design Rationale table. The reason for the move is itself recorded in the stories folder.

### 3.5 Summary section

Include this section only in documents with many small rules that a reader might want to scan quickly.

Format as a two-column table: left column is the concept name, right column is the rule in one sentence.

```markdown
## N. Summary

| Concept | Rule |
|---|---|
| Class body | Fields only — no methods, no constructors |
| Constructor | Package-scope declaration; no `this`; returns `init{ }` |
```

---

## 4. Language Comparisons Section

### 4.1 Feature matrix

A multi-column table comparing Zane against other languages on a set of boolean features.

```markdown
| Feature | Zane | C | Go | Rust |
|---|---|---|---|---|
| Unhandled errors are compile-time errors | ✅ | ❌ | ❌ | ✅ |
| Zero-cost (no stack unwinding) | ✅ | ✅ | ✅ | ✅ |
```

Use `✅` and `❌`. Use `⚠️ Note` for partial support.

### 4.2 Per-language comparisons

One subsection per language, formatted as:

1. A one-sentence description of that language's approach.
2. A side-by-side code block — the other language first, then Zane.
3. A two-column problem/solution table.

Example format (note: this example is fenced with four backticks so the inner code fences render correctly):

````markdown
### N.M Zane vs. Go (Multiple Return Values)

Go signals errors through a second return value that the caller is free to ignore.

**Go:**
```go
content, err := os.ReadFile("file.txt")
if err != nil { return "", err }
```

**Zane:**
```zane
 content String = fs:readFile("file.txt") ? err { abort err }
```

| Problem in Go | How Zane Solves It |
|---|---|
| The compiler does not enforce that you check the error return. | Every abortable call **must** have a `?` handler. |
````

The table header is always one of:
- `| Problem in <Language> | How Zane Solves It |` (problem/solution framing), or
- `| Difference | <Language> | Zane |` (neutral comparison framing).

---

## 5. `syntax.md` Conventions

`syntax.md` is the canonical grammar reference. It contains *only* surface syntax — the form of things, not their meaning.

### 5.1 What belongs in `syntax.md`

- Grammar production rules written as annotated code blocks.
- Legal and ILLEGAL examples of every syntactic form.
- Positional grammar: where keywords appear relative to each other.
- Nothing about what a construct *does* — that belongs in the topic docs.

### 5.2 What does not belong in `syntax.md`

- Semantics, type rules, or compiler behaviour.
- Design reasoning (the why — that lives in `stories/`) or language comparisons.
- Anything that requires explaining *why*.

### 5.3 Referencing `syntax.md` from topic docs

In a topic doc, replace inline syntax descriptions with a cross-reference:

```markdown
> **See also:** [`syntax.md`](syntax.md) §1 and §3 for the complete declaration forms.
```

### 5.4 Code block language tags

Use the `zane` tag for all Zane source examples and pseudo-grammar forms:

````markdown
```zane
maxHp Int(100)
hp = computeHp()
```
````

Pseudo-grammar forms also use the `zane` tag:

````markdown
```zane
name Type(arg, ...)
name = expr
```
````

When showing declaration syntax, avoid examples that look like a symbol is being re-declared. If the symbol already exists, use `name = expr` rather than `name Type`, `name Type(...)`, or `name Type{...}`.

Use the appropriate language tag (`c`, `go`, `rust`, `swift`, `python`, `zig`) for examples in Language Comparison sections.

### 5.5 ILLEGAL examples

Mark clearly illegal syntax or semantics with a `// ILLEGAL` or `// compile error` comment:

```zane
Int[Node, Int] mut   // ILLEGAL: mut requires this as first parameter
Void[Int, this Node] // ILLEGAL: this must be the first parameter
```

---

## 6. Prose Style

### 6.1 Sentence length and density

Keep sentences short. One idea per sentence. Avoid nested clauses. Use active voice.

Good: *The compiler nulls all refs to the object via the anchor.*
Bad: *The refs that are registered against the anchor of the object that was destroyed are nulled by the anchor mechanism.*

### 6.2 Emphasis

Use `**bold**` for the first occurrence of a term being defined or for a key constraint.  
Use `` `backtick` `` for all code identifiers, keywords, operators, and type names.  
Do not use *italics* for emphasis. Italics are reserved for the names of other documents or for semantic categories the user is not expected to write (e.g., *Total Pure*).

### 6.3 Neutral register

A spec document states rules without arguing for them and without editorial voice. No first person, no "we chose", no "unfortunately". Judgement, opinion, and honest doubt about a decision belong in the stories doc, where the register is explicitly looser (see [`writing-stories-docs.md`](writing-stories-docs.md) §6).

### 6.4 Cross-references

Always link by filename, never by section title text. Include a `§` number after the link:

```markdown
[`memory.md`](memory.md) §3
```

When section numbers change, update inbound and outbound references in the same change. Stories docs do not need updating for a section renumber: a story's spec references are commit-pinned permalinks frozen to the spec as it was (see [`writing-stories-docs.md`](writing-stories-docs.md) §4.2), and the spec's own `> **Story:**` pointers link to the story file as a whole, not to a section number.

To reduce churn:
- Prefer adding new subsections to the end of an existing section when possible.
- Only renumber when necessary (but if you renumber, keep numbering contiguous).

At the end of a section that is closely connected to another document, add a `> **See also:**` line:

```markdown
> **See also:** [`effects.md`](effects.md) for the complete effect model.
```

---

## 7. Adding a New Spec Document

1. Create `spec/<topic>.md` (kebab-case file name, no `_model` or `_handling` suffixes; the file name should match what readers will type when looking for it).
2. Follow the required shape from §2.
3. Add a row to the appropriate table in [`README.md`](../README.md).
4. If the document introduces new syntax forms, add them to `syntax.md` and cross-reference from the topic doc.
5. Create the matching `stories/<topic>.md` stories doc (see [`writing-stories-docs.md`](writing-stories-docs.md)) and add `> **Story:**` pointers from the non-trivial sections.
6. If meaningful language comparisons exist, add a Language Comparisons section (§4 format).

Exception: if the document is `glossary.md`, follow the glossary-specific reference shape from §2.6 instead of the topic-document layout. Record each term's meaning, why the name fits, and the canonical home document. Do not add a Language Comparisons section. Reference documents (`syntax.md`, `glossary.md`) do not get a stories doc.

---

## 8. Editing an Existing Document

- Do not remove section numbers — renumber instead (and update references; stories docs need no update on a renumber — see §6.4).
- Do not add `---` between subsections.
- Do not add semantics to `syntax.md`.
- Do not duplicate content between files — add it in the canonical place and cross-reference from others.
- When you add or change a rule whose reasoning is non-trivial, extend the narrative in the matching `stories/<topic>.md` and add or update its `> **Story:**` pointer in the same change.
- When adding a new top-level section, insert it before Language Comparisons / Summary as required by §2, adjusting section numbers accordingly.
