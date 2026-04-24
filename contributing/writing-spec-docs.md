# Writing Zane Spec Docs

This guide describes the conventions for writing and maintaining spec documents in this repository. Follow it when adding a new document, extending an existing one, or editing any section.

---

## 1. Document Inventory

Each file in the repository root covers one topic area. No two files overlap in purpose.

| File | Purpose |
|---|---|
| `memory_model.md` | Ownership, destruction, heap layout, anchor/ref system |
| `oop.md` | Classes, structs, constructors, methods, overloading, packages |
| `purity.md` | Effect model, `mut`, inferred purity levels, capabilities |
| `error_handling.md` | Bifurcated return paths, `?` handler, `??`, `resolve`/`abort` |
| `dependency_management.md` | Manifest, CLI, symbol versioning, package cache, build flow |
| `syntax.md` | Surface syntax only — grammar forms and code patterns; no semantics |

When in doubt about where a piece of content belongs, ask: does it describe *what the language does* (topic doc) or *how to write it* (syntax.md)?

---

## 2. Required Document Shape

Every **topic spec document** must follow this top-to-bottom shape exactly:

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

## N. Design Rationale

Rationale table (see §4).

---

## N+1. Language Comparisons    ← only if comparisons apply to this document

Comparison tables and per-language breakdowns (see §5).

---

## N+2. Summary    ← only in topic docs with many rules

One-column summary table.
```

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
> **See also:** [`oop.md`](oop.md) for class/struct declarations and method syntax. [`purity.md`](purity.md) for the effect model.
```

Use `>` blockquote with bold `**See also:**`. Link text is the filename. Description is a short phrase. Separate entries with `. ` (period space).

### 2.4 Section separator

Use a bare `---` line between every top-level section (`## N.`). Never put `---` between subsections (`### N.M`).

### 2.5 Section numbering

Top-level sections are numbered `## 1.`, `## 2.`, etc. Subsections are `### 1.1`, `### 1.2`, etc. Do not skip numbers. Do not restart numbering. In topic documents, the Overview is always `## 1.`.

### 2.6 `syntax.md` is the one structural exception

`syntax.md` is a canonical reference document, not a topic narrative. It still uses the same title style, lead-in prose, `---` separators, and numbered subsections, but it may start directly with grammar categories such as Declarations, Types, and Calls instead of a dedicated `## 1. Overview` section.

---

## 3. Section Types

### 3.1 Overview

Every topic document begins with `## 1. Overview`. It contains:

- A short paragraph explaining what the feature is.
- A bullet list of 2–5 core ideas, each formatted as **`keyword`**. Brief explanation.

```markdown
## 1. Overview

Zane uses a **structural effect model** with a single user-facing effect modifier: `mut`.

- **Single ownership.** Every heap object has exactly one owner at all times.
- **Anchor-based refs.** A `ref` points through a stable anchor, never directly at an object.
```

### 3.2 Core topic sections

Numbered `## 2.` onward. Write prose that explains *what the language does* and *why*, grounded in the rules. Include code examples in every subsection that has a non-obvious behaviour.

Do not include syntax grammar forms in topic docs. Those go in `syntax.md`. Cross-reference with:

```markdown
> See [`syntax.md`](syntax.md) §3.2 for the declaration grammar.
```

### 3.3 Design Rationale section

Required in every topic document. Always the penultimate top-level section (one before Summary if a Summary exists, or the last section if there is no Summary).

See §4 for format.

### 3.4 Language Comparisons section

Required only in documents where Zane's design is meaningfully different from mainstream alternatives. Currently present in `memory_model.md` and `error_handling.md`. See §5 for format.

### 3.5 Summary section

Required only in documents with many small rules that a reader might want to scan quickly. Format as a two-column table: left column is the concept name, right column is the rule in one sentence.

```markdown
## N. Summary

| Concept | Rule |
|---|---|
| Class body | Fields only — no methods, no constructors |
| Constructor | Package-scope declaration; no `this`; returns `init{ }` |
```

---

## 4. Design Rationale Section

Format as a two-column markdown table: `Decision` | `Rationale`.

```markdown
## N. Design Rationale

| Decision | Rationale |
|---|---|
| Single ownership by default | Eliminates ambiguity about which variable owns an object. ... |
| `ref` as explicit opt-in | Ownership is the safe default. Non-owning access is the exception. ... |
```

Rules:
- Each row is one atomic design decision.
- The Decision cell is short — a noun phrase or keyword in backticks.
- The Rationale cell is one or two sentences explaining the *why*, not restating the *what*.
- Order rows by conceptual importance, not by document section order.
- Do not include a `|---|---|` separator between groups. One unbroken table per document.

---

## 5. Language Comparisons Section

### 5.1 Feature matrix

A multi-column table comparing Zane against other languages on a set of boolean features.

```markdown
| Feature | Zane | C | Go | Rust |
|---|---|---|---|---|
| Unhandled errors are compile-time errors | ✅ | ❌ | ❌ | ✅ |
| Zero-cost (no stack unwinding) | ✅ | ✅ | ✅ | ✅ |
```

Use `✅` and `❌`. Use `⚠️ Note` for partial support.

### 5.2 Per-language comparisons

One subsection per language, formatted as:

1. A one-sentence description of that language's approach.
2. A side-by-side code block — the other language first, then Zane.
3. A two-column problem/solution table.

```markdown
### N.M Zane vs. Go (Multiple Return Values)

Go signals errors through a second return value that the caller is free to ignore.

**Go:**
```go
content, err := os.ReadFile("file.txt")
if err != nil { return "", err }
```

**Zane:**
```zane
content String
content = fs:readFile("file.txt") ? err { abort err }
```

| Problem in Go | How Zane Solves It |
|---|---|
| The compiler does not enforce that you check the error return. | Every abortable call **must** have a `?` handler. |
```

The table header is always `| Problem in <Language> | How Zane Solves It |` for problem/solution framing, or `| Difference | <Language> | Zane |` for neutral feature comparisons.

---

## 6. `syntax.md` Conventions

`syntax.md` is the canonical grammar reference. It contains *only* surface syntax — the form of things, not their meaning.

### 6.1 What belongs in `syntax.md`

- Grammar production rules written as annotated code blocks.
- Legal and ILLEGAL examples of every syntactic form.
- Positional grammar: where keywords appear relative to each other.
- Nothing about what a construct *does* — that belongs in the topic docs.

### 6.2 What does not belong in `syntax.md`

- Semantics, type rules, or compiler behaviour.
- Rationale or language comparisons.
- Anything that requires explaining *why*.

### 6.3 Referencing `syntax.md` from topic docs

In a topic doc, replace inline syntax descriptions with a cross-reference:

```markdown
> See [`syntax.md`](syntax.md) §3.2 for the complete declaration grammar.
```

### 6.4 Code block language tags

Use the `zane` tag for all Zane source examples:

````markdown
```zane
maxHp Int(100)
hp = computeHp()
```
````

Use no tag (plain fence) for pseudo-grammar forms:

````markdown
```
name Type(arg, ...)
name = expr
```
````

When showing declaration syntax, avoid examples that look like a symbol is being re-declared. If the symbol already exists, use `name = expr` rather than `name Type`, `name Type(...)`, or `name Type{...}`.

Use the appropriate language tag (`c`, `go`, `rust`, `swift`, `python`, `zig`) for examples in Language Comparison sections.

### 6.5 ILLEGAL examples

Mark clearly illegal syntax or semantics with a `// ILLEGAL` or `// compile error` comment:

```zane
(Node, Int) mut -> Int   // ILLEGAL: mut requires this as first parameter
(Int, this Node) -> Void // ILLEGAL: this must be the first parameter
```

---

## 7. Prose Style

### 7.1 Sentence length and density

Keep sentences short. One idea per sentence. Avoid nested clauses. Use active voice.

Good: *The compiler nulls all refs to the object via the anchor.*
Bad: *The refs that are registered against the anchor of the object that was destroyed are nulled by the anchor mechanism.*

### 7.2 Emphasis

Use `**bold**` for the first occurrence of a term being defined or for a key constraint.
Use `` `backtick` `` for all code identifiers, keywords, operators, and type names.
Do not use *italics* for emphasis. Italics are reserved for the names of other documents or for semantic categories the user is not expected to write (e.g., *Total Pure*).

### 7.3 Cross-references

Always link by filename, never by section title text. Use a `§` number after the link:

```markdown
[`memory_model.md`](memory_model.md) §3
```

At the end of a section that is closely connected to another document, add a `> See also:` line:

```markdown
> **See also:** [`purity.md`](purity.md) for the complete effect model.
```

---

## 8. Adding a New Spec Document

1. Create `<topic>.md` in the repository root.
2. Follow the required shape from §2 exactly.
3. Add a row to the table in `README.md`.
4. If the document introduces new syntax forms, add them to `syntax.md` and cross-reference from the topic doc.
5. If the document has design rationale, add a Design Rationale section (§4 format).
6. If meaningful language comparisons exist, add a Language Comparisons section (§5 format).

---

## 9. Editing an Existing Document

- Do not remove section numbers — renumber instead.
- Do not add `---` between subsections.
- Do not add semantics to `syntax.md`.
- Do not duplicate content between files — add it in the most specific place and cross-reference from others.
- When adding a new top-level section, insert it before Design Rationale and Language Comparisons, adjusting their numbers accordingly.
