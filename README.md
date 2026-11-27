<!-- markdownlint-disable-next-line MD041 -- the logo floats before the title -->
<img height="96" align="right" src="https://raw.githubusercontent.com/zane-lang/logos/main/zane/zane.svg" alt="Zane logo" />

# The Zane Language Specification

This repository defines the [Zane](https://github.com/zane-lang/zane) programming language: the meaning of every construct, the set of legal programs, and how memory, effects and packages work. Next to the specification sit the **design stories**, which record how and why each part came to be.

The specification is a reference, not a tutorial. To learn the language first, start at <https://zane-lang.org/docs>.

This repository is also a **prior art publication**. The techniques described in these documents were conceived and developed by the author and are dedicated to the public domain under [CC0 1.0 Universal](LICENSE). Anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author.

## Where to start

[`spec/README.md`](spec/README.md) lists every specification document by topic, with a reading order; [`stories/README.md`](stories/README.md) does the same for the design stories. Both are also published, with search, at <https://zane-lang.org/spec> and <https://zane-lang.org/stories>.

## Repository layout

```text
spec/             the specification itself: what the language does
stories/          design stories: how each part of the spec came to be
contributing/     style guides for writing spec and stories documents
memorybench/      a C harness that models the memory design
langbench/        compiled Zane programs timed against the same programs in other languages
site/             the page that publishes both benchmarks
```

The benchmarks' pinned results are published at <https://zane-lang.github.io/spec/>. [`memorybench/`](memorybench/) measures a C model of the memory design ([`spec/memory.md`](spec/memory.md)); [`langbench/`](langbench/README.md) measures what a released compiler builds.

## Contributing

Read the guide for the kind of document you are editing first:

- [`contributing/writing-spec-docs.md`](contributing/writing-spec-docs.md): normative spec documents in [`spec/`](spec/).
- [`contributing/writing-stories-docs.md`](contributing/writing-stories-docs.md): stories in [`stories/`](stories/).
- [`contributing/naming-terms.md`](contributing/naming-terms.md): how the coined terms in [`glossary.md`](spec/glossary.md) are chosen.
- [`contributing/markdown-formatting.md`](contributing/markdown-formatting.md): the one Markdown rule CI checks, and how to run the check.
