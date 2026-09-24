# Filing and Closing Issues

This guide describes how issues on this repository are labelled and closed. Most
issues here are **proposals** — a suggested change to the language or to how the
spec describes it — and the rest are defects in the spec's text and tasks on the
repository itself. A proposal's outcome is recorded by a label, because GitHub's
close reasons are a fixed set of three and cannot name every way a proposal ends.

---

## 1. Proposals

A proposal carries the `proposal` label from the moment it is filed. While it is
open, it is under discussion.

When the discussion ends, the proposal receives exactly one outcome label, which
fixes the close reason it takes:

| Outcome label | Meaning | Close as |
| --- | --- | --- |
| `proposal: accepted` | The design takes the proposal; the spec changes to carry it. | completed |
| `proposal: rejected` | The proposal is sound, and the design declines it. | not planned |
| `proposal: invalid-premise` | The proposal rests on a reading of the spec, or an assumption about the language, that turned out to be wrong, so the question it raises does not arise. | not planned |

`rejected` and `invalid-premise` share a close reason and differ in what they
say about the idea: a rejected proposal is a road the design considered and did
not take, which is exactly what a story records, while an invalid premise is a
misunderstanding the spec's text may need to prevent. The closing comment says
which, and for a rejection, why.

An accepted proposal stays open until the spec change that carries it merges,
and that pull request closes it.

A proposal that repeats an earlier one is closed as **duplicate** of it and
takes no outcome label; the earlier issue carries the outcome.

## 2. Finding past decisions

Every proposal the design declined is one filter away:

- [Rejected proposals](https://github.com/zane-lang/spec/issues?q=is%3Aissue+label%3A%22proposal%3A+rejected%22)
- [Accepted proposals](https://github.com/zane-lang/spec/issues?q=is%3Aissue+label%3A%22proposal%3A+accepted%22)
- [Open proposals](https://github.com/zane-lang/spec/issues?q=is%3Aissue+is%3Aopen+label%3Aproposal)

Search the rejected list before filing a proposal: when an idea has already been
declined, reopening that issue with what has changed since carries the earlier
discussion forward.

## 3. Defects and tasks

An issue that reports a contradiction, an error, or a gap in the spec's text,
or that tracks work on the repository itself, carries no `proposal` label and
closes the ordinary way — **completed** when fixed, **not planned** when it will
not be done.
