# Zane Control Flow

This document specifies Zane's control-flow constructs: conditional branching, scope-exit guards, counted loops, and the language's 1-based ordinal convention.

> **See also:** [`syntax.md`](syntax.md) §5 for the canonical surface syntax. [`operators.md`](operators.md) §2.4 for `and` and `or`.

---

## 1. Overview

Zane keeps control flow small and explicit. Branching uses `if`/`elif`/`else`, early scope exit uses `guard`, and repetition uses bounded `loop`.

- **`Bool conditions`.** `if`, `elif`, and `guard` conditions are `Bool` expressions.
- **`Guard exits the current scope`.** `guard` leaves the enclosing lexical scope instead of introducing another nested branch.
- **`Bounded loops`.** `loop` always has a written upper bound; Zane does not define a separate `while`.
- **`1-based ordinals`.** Counted loops and positional indexing start at `1`, not `0`.

---

## 2. Conditional Branching

### 2.1 `if` chains evaluate top to bottom
An `if` chain evaluates its conditions from top to bottom. The first branch whose condition is `true` runs. If no prior condition is `true` and an `else` branch is present, the `else` branch runs.

All `if` and `elif` conditions **MUST** have type `Bool`.

```zane
if ready {
    start()
} elif retryAllowed and cached {
    useCache()
} else {
    fail()
}
```

### 2.2 `elif` is the continuation form
Zane uses the single keyword `elif` for chained conditions. `else` is the unconditional fallback branch and appears only at the end of the chain.

```zane
if firstChoice {
    pickA()
} elif secondChoice {
    pickB()
} else {
    pickDefault()
}
```

---

## 3. `guard` Scope Exit

### 3.1 `guard` exits when its condition is true
`guard condition` immediately exits the current lexical scope when `condition` evaluates to `true`. When the condition is `false`, execution continues with the next statement in the same scope.

```zane
{
    value Int(3)
    guard finished
    print(value)
}
```

In the example above, `print(value)` runs only when `finished` is `false`.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-that-opens-no-scope-of-its-own) — "An exit that opens no scope of its own".

### 3.2 `guard` may run a pre-exit block
`guard condition { ... }` first executes the attached block and then exits the same enclosing scope when the condition is `true`. If the condition is `false`, the attached block is skipped.

```zane
{
    value Int(3)
    guard shouldExit {
        print(value)
        print("pre-exit")
    }
    print("still inside")
}
```

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#an-exit-that-opens-no-scope-of-its-own) — "An exit that opens no scope of its own".

---

## 4. Counted Loops

### 4.1 `loop from ... to ...` is inclusive
`loop name from start to end { ... }` iterates over an inclusive integer range. On each iteration, `name` is bound to the current `Int` value, starting at `start`, increasing by `1`, and ending at `end`.

```zane
loop i from 1 to 3 {
    print(i)
}
```

The loop above visits `i = 1`, then `2`, then `3`.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#counting-from-one) — "Counting from one".

### 4.2 `loop ... to ...` starts at `1`
`loop name to end { ... }` is shorthand for `loop name from 1 to end { ... }`.

```zane
loop i to 3 {
    print(i)
}
```

This loop runs exactly three times and visits `i = 1`, `2`, and `3`.

### 4.3 There is no dedicated `while`
Zane does not define a separate `while` form. Repetition has a written bound through `loop`; a condition checked during repetition is written explicitly in the body through `guard`.

```zane
loop i from 1 to 10 {
    guard reachedLimit
    work(i)
}
```

When `reachedLimit` becomes `true`, the `guard` exits the loop body scope and the loop stops.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#doing-without-while) — "Doing without `while`".

---

## 5. 1-Based Ordinal Counting

### 5.1 Positional indexing is 1-based
When an `Int` identifies an ordinal position in an ordered sequence, the first position is `1`. For a sequence with `n` elements, the positional index range is therefore `1` through `n`.

```zane
lastPosition Int = list:size()
element Int = list[lastPosition]
```

The example above selects the last element by using the sequence size directly as the final valid position.

> **Story:** [`stories/control-flow.md`](../stories/control-flow.md#counting-from-one) — "Counting from one".

### 5.2 Bounds behavior remains separate
This document specifies the ordinal base only. The language-level behavior for out-of-range element access remains a separate question from whether indexing starts at `0` or `1`.
