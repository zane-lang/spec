# Markdown formatting

Every heading in every Markdown file here — `spec/`, `stories/`, `contributing/`, and the README — carries a blank line above and below it, at every heading level. The file's own edges are the exception: a title on line 1 has nothing above it to separate it from, and a heading that ends a file has nothing below. That is markdownlint's `MD022`, and the committed [`.markdownlint.jsonc`](../.markdownlint.jsonc) is the canonical statement of it. It is also the *only* rule this repository has taken a position on: everything else markdownlint ships is switched off there, so a clean run means the headings are right and says nothing about the rest. Check a change with

```sh
devbox run lint
```

On Intel macOS, [Devbox currently fails because its default Nixpkgs no longer supports that platform](https://github.com/jetify-com/devbox/issues/2937). With Node.js installed, the same linter and committed rules can be run directly:

```sh
npx --yes markdownlint-cli2 "**/*.md"
```

A [workflow](../.github/workflows/markdownlint.yml) runs that same command on every pull request. It names no rules itself — it finds the config the way the local run does.
