# Working in the Zane spec repo

This repo is the Zane language specification. `spec/` holds the normative rules
(*what*), `stories/` holds the design history (*how/why*), and `contributing/`
holds the style guides for each. Read the relevant guide in `contributing/`
before editing or adding a document.

## PR reviews: the `gemini-code-assist` bot

A `gemini-code-assist` reviewer is installed on PRs. It is **conservative and
defends the status quo** — it frequently objects to the very change a PR is
making, by citing the existing rule or design as if it were immutable.

It is installed deliberately, but for the *common* case: PRs that work **within**
the established design (new spec sections, additional rules, clarifications),
where guarding against accidental drift is useful. It is **not** a good judge of
PRs whose explicit purpose is to **change a fundamental convention or design
decision** — there it will reliably argue against the change itself.

So when handling its review comments:

- If the PR works within the existing design, weigh its comments normally.
- If the PR's intent is to *change* a rule/convention and the bot objects by
  appealing to that very rule, that objection is expected and can be declined —
  it is arguing against the point of the PR, not finding a real problem. Don't
  revert the core of the change to satisfy it. Reply briefly explaining the
  intent (once, even if it left several near-duplicate comments) and resolve the
  threads as addressed-by-design.

The maintainer (repo owner) is the authority on design decisions; the bot is not.
