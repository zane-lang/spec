# Zane Algebraic Data Types

This document specifies Zane's algebraic data types: the `enum` of uniform peer members, the `variant` sum mould, the body symmetry that ties `variant` to `struct`, matching a variant through the central `match` block, and enum maps.

> **See also:** [`types.md`](types.md) for value and reference types and constructors. [`syntax.md`](syntax.md) §1 and §4.8 for the surface forms. [`memory.md`](memory.md) §2.10 for why a value type cannot hold `&` or recurse. [`generics.md`](generics.md) §7 for the uniform-stride rule. [`error-handling.md`](error-handling.md) §3.5 for `?` handlers on `match`. [`lexical.md`](lexical.md) §3 and §6 for casing and delimiters.

---

## 1. Overview

Zane separates two ideas that other languages often merge. An `enum` is a closed set of interchangeable, payloadless peers. A `variant` is a sum mould whose members each carry a payload. Keeping them distinct lets each stay simple: enums are uniform and tabulatable, variants are heterogeneous and dispatched.

- **`enum is uniform peers`.** A closed set of lowercase, payloadless members that mean one uniform thing. Per-member data is attached externally by an enum map.
- **`variant is a sum mould`.** A value of the type it declares holds exactly one of its named members. Its body grammar is byte-for-byte identical to a `struct`; the keyword flips product into sum.
- **`The # axis applies to the sum mould`.** A plain `variant` is its value form; `#variant` is its reference form (see [`types.md`](types.md) §2.1). The `#` mark applies the same way to an `enum`.
- **`Reading a variant member is partial`.** A case may not be live, so a member read is abortable. The primary consumer is exhaustive dispatch.
- **`A variant is matched in one central block`.** A `match` block (§5) dispatches a variant on its live tag — variant matching, not pattern matching: no nested destructuring, guards, or shape tests — and must cover every case, with no default arm.
- **`Recursion requires a reference type`.** A recursive sum must be a `#variant`, never a value `variant`, because a value type is transitively value and cannot hold the `&` a recursive member boxes through.

---

## 2. Enums

An `enum` is a closed set of **interchangeable peer** members that mean one uniform thing — colors, brands, weekdays. It is **not** a sum mould. Its members are lowercase, payloadless peer values, written as a flat `[ ]` list.

```zane
type Operator = enum [ add, sub, mul, div, eq, lessEq, moreEq, less, more ]

type Colors = enum [ red, green, blue ]
```

A member is accessed as a value through the type name: `Colors.red`. The type is uppercase and the member is lowercase, so member selection (`Colors.red`) reads exactly like field selection (`vec.x`) and the casing rule (see [`lexical.md`](lexical.md) §3) is never broken.

### 2.1 Why an enum is not a variant

The property that distinguishes an `enum` is **uniformity** — the substitutability of intent — not structure. Enum members are peers you can iterate, ordinal-index, and build total tables over. The moment a case needs its own heterogeneous payload, it is a `variant`, not an `enum`.

Per-member associated data is attached externally through an enum map (§6), which keeps the members themselves payloadless and interchangeable. The consumers of an enum are iteration, ordinal use, total mapping, and exhaustive matching.

> **Story:** [`stories/adt.md`](../stories/adt.md#two-constructs-against-the-hype) — "Two constructs, against the hype".

---

## 3. Variants

A `variant` is a **sum mould**. A value of the type it declares holds **exactly one** of the variant's named members at a time. The body uses `{ }` brackets with `;`-terminated members, each a lowercase member name followed by its payload type — the same grammar a `struct` uses.

A plain `variant` is a **value** sum: copied on assignment, transitively value, non-recursive. A `#variant` is a **reference** sum: it has identity, may hold reference-type and `&` payloads, and may recurse (§4). A recursive sum such as `Expr` — whose members refer back to `Expr` through `&` — must therefore be a `#variant`:

```zane
type QualifiedIdent = struct { packageName String; member String; }
type BinOp = #struct { left &Expr; right &Expr; operator Operator; }

type Expr = #variant {
    intLit String;
    floatLit String;
    strLit String;
    boolLit Bool;
    ident String;
    qualifiedIdent QualifiedIdent;
    op BinOp;
    flip &Expr;
    parenthesized &Expr;
    funcCall FuncCall;
    funcLambda FuncLambda;
    methLambda MethLambda;
}
```

A member projected as a type is written `Expr.intLit`: `Expr` is the type (uppercase) and `.intLit` is member selection (lowercase), exactly like `vec.x`.

Reading a member of a variant value is **partial**: the case may not be the live one. A member read is therefore an **abortable** access (`?` / `??`, see [`error-handling.md`](error-handling.md)). The primary consumer of a variant is the exhaustive `match` block (§5). A single-payload case, once bound, behaves as its payload, so a value of `Expr.intLit`'s payload type reaches that payload's members directly.

### 3.1 The struct/variant symmetry

A `struct` and a `variant` share one declaration body. The keyword flips four things in lockstep. "Interchangeable" applies to the *declaration*, not to consuming code: construction and reads differ between the two.

| | `struct { a A; b B; }` (product) | `variant { a A; b B; }` (sum) |
|---|---|---|
| Meaning | has `a` **and** `b` | has `a` **or** `b` |
| Construct | must set **all** members with `init{ }` | names **exactly one** case and supplies its payload (§3.2) |
| Read `e.a` | total — always an `A` | partial — abortable, an `A` only if `a` is live |
| Layout | sum of member sizes | tag + max of member sizes |

> **See also:** [`types.md`](types.md) §2.5 for the same relationship from the struct side.

> **Story:** [`stories/adt.md`](../stories/adt.md#one-body-product-or-sum) — "One body, product or sum".

### 3.2 Constructing a variant

A variant value is built by **naming a case and supplying its payload**: `Expr.intLit("5")` selects the `intLit` case, gives it its payload, and produces an `Expr` with that case live. The form is `<Variant>.<case>(payload)` — `Expr` is the type (uppercase) and `.intLit` the case (lowercase), the same `Type.member` selection that reads a case and that names an `enum` member (§2). A case carries **exactly one** payload type (§3), so the form takes **exactly one** argument.

This is built-in syntax, **not** a call to a constructor verb. A `struct` is built by a constructor that sets every field with `init{ }` (see [`types.md`](types.md) §3); a variant has cases, not fields, so it is written directly, by selecting one case. That is the construction half of the struct/variant symmetry (§3.1): one body grammar, opposite construction.

The form is an ordinary **expression** of the variant type, legal wherever a value is — an initializer, a `return`, a call argument, or the payload of another case form. It yields the whole variant, so a variable built this way holds the variant type, not a per-case type:

```zane
e Expr = Expr.intLit("5")   // e is an Expr, intLit live
e Expr.intLit("5")          // shorthand: the instantiation form (syntax.md §1.1), e is still an Expr
```

The two lines declare the same thing. The trailing argument list is what marks construction: `Expr.intLit(...)` builds and yields an `Expr`, distinct from a bare `Expr.intLit` written as a projected case *type* (§3). The `.intLit` chooses which case is live; it does not narrow the variable, because the value it produces is the variant.

The payload argument sits at a positional **coercion site** (see [`types.md`](types.md) §4.2), so an applicable `implicit` constructor is inserted there exactly as at any other call argument.

A payload that is itself a constructed value — a `struct` or another variant — is built on its own and passed in. Construction **nests**; it never dots through:

```zane
e Expr.op(BinOp.fromParts(a, b))              // build the BinOp payload, then wrap it
e Expr.qualifiedIdent(QualifiedIdent{ packageName, member })
```

Naming a case takes its payload whole; to reach a nested case, write another case form for the payload. There is no `Expr.op.fromParts(...)` reaching into a payload's own construction, and no `Outer.a.b(...)` chaining through one case into another — the mirror, on the construction side, of matching one level and going no deeper (§5.3).

An `enum` member is the payloadless degenerate of the same form: `Colors.red` selects a case that carries no payload, so it is written with no argument list (§2). A payload-carrying case is called; a payloadless one is selected.

A recursive `#variant` case boxes through `&` (§4), and its construction follows the ordinary reference rules: `Expr.flip(r)` takes an `&Expr`, and its argument must be a source that may create a new `&` (see [`memory.md`](memory.md) §2.8), exactly as an `&` field of a `#struct` requires (see [`types.md`](types.md) §3.9).

**Shared surface, different mechanism.** The `Type.member(args)` form — in both its long (`e Expr = Expr.intLit("5")`) and short (`e Expr.intLit("5")`) declaration — is exactly the surface a **named constructor** on a product type uses (see [`types.md`](types.md) §3.4): `v Vector2.diagonal(Float(3))` reads and declares just like `e Expr.intLit("5")`. The resemblance is purely **syntactic**. A named constructor is a declared *verb* that builds through `init{ }`; naming a variant case is built-in syntax with no verb behind it. They share a spelling, not a mechanism.

> **Story:** [`stories/adt.md`](../stories/adt.md#naming-a-case-not-calling-a-constructor) — "Naming a case, not calling a constructor".

---

## 4. Recursion and Storage

A directly inline self-reference would have infinite size, which the uniform-stride rule forbids (see [`generics.md`](generics.md) §7). A recursive member must therefore **box through `&`**: directly, as `flip &Expr`, or inside a named reference type it names, as `op BinOp` where `BinOp` holds `left &Expr; right &Expr;`.

- A value type — `struct` or `variant` — **cannot** hold an `&` or contain itself (see [`memory.md`](memory.md) §2.10). A recursive type must therefore be a reference type: a `#variant` or a `#struct`, **never** a value type. The body syntax is symmetric across all four kinds; the `#` modifier decides which may recurse.
- The `#` modifier is what carries recursion: a plain `variant` is the sum mould's value form, laid out inline, while a `#variant` is its reference form — carrying a tag, boxing its recursive cases through `&`, and placed by the ordinary reference-type rules ([`memory.md`](memory.md) §3.5). A recursive sum such as `Expr` is a `#variant`.
- Indirection is always **explicit `&`**. There is no hidden auto-boxing, matching Zane's stance that hosting and guests are explicit.

> **Story:** [`stories/adt.md`](../stories/adt.md#one-body-product-or-sum) — "One body, product or sum".

---

## 5. Matching a Variant

A `variant` is consumed by a **`match` block**: one central place that lays out every case and dispatches on the live tag. It is Zane's single construct for consuming a variant. A `match` produces the selected arm's value and may appear anywhere an expression is legal, including an initializer, return expression, or call argument.

```zane
result String = match e {
    x strLit                   => x;
    [intLit, floatLit]         => "number";
    x [ident, qualifiedIdent]  => nameOf(x);
    b op                       => render(b);
    [boolLit, flip, parenthesized, funcCall, funcLambda, methLambda] => "other";
}

print(match e {
    x strLit                   => x;
    [intLit, floatLit]         => "number";
    [boolLit, ident, qualifiedIdent, op, flip, parenthesized, funcCall, funcLambda, methLambda] => "other";
})
```

> **See also:** [`syntax.md`](syntax.md) §4.8 for the surface grammar.

### 5.1 The block and its arms

The scrutinee is followed by a `{ }` block of `;`-terminated **arms**, the same terminator a `struct`/`variant` body uses. An arm is an optional binder, a case selector, `=>`, and a body:

```zane
[binder] selector => body ;
```

- **Optional binder.** `x strLit => body` binds the payload as `x`; a bare `strLit => body` handles the case without binding. An arm head reads exactly like a declaration `x VarType`, with the `Expr.` supplied by the scrutinee — the casing rule (see [`lexical.md`](lexical.md) §3) carries it with no new syntax.
- **Selector.** A single case name (`strLit`) or a `[ ]` group of `,`-separated case names (`[intLit, floatLit]`), written bare like an enum member.
- **Binder type.** A single-case binder behaves as that case's payload (§3). A `[ ]` group is **shorthand for one arm per listed case**: `x [ident, qualifiedIdent] => body` expands to `x ident => body;` and `x qualifiedIdent => body;`, each binding `x` at *its own* case's payload. A single case and a group are therefore the same mechanism — a group is just the arm written once. The bracket is a **case selector, not a type**: no anonymous sub-variant is formed, so every type stays named (see [`types.md`](types.md) §1). Because the binder is always a case's payload, it never stands for the whole variant; when an arm needs the variant itself, it names the scrutinee, which stays in scope.
- **Body.** `=> expr` is shorthand for `{ return expr }`; a larger arm uses a `{ }` block body.
- **One result type.** All arms share one return type, which is the type of the `match`.

Because a group is just its arms written once, its cases need not share a payload type. Each expanded arm is checked independently against its own case's payload, so an operation on the binder — `nameOf(x)` above — must resolve for every grouped payload, ordinarily by being overloaded across them. A grouped arm that only ever wants the whole variant simply omits the binder and reads the scrutinee. Zane has no interfaces or constraints over arbitrary types, so a binder shared across *differing* payloads is useful exactly where such an overload family exists, and a heterogeneous "everything else" group is normally left unbound.

A scrutinee may also be an `enum` rather than a `variant`. Its members are payloadless, so each arm is a bare member (or `[ ]` group) with no binder; this is the enum's exhaustive-matching consumer (§2.1).

> **Story:** [`stories/adt.md`](../stories/adt.md#the-group-is-sugar-not-a-widening) — "The group is sugar, not a widening".

### 5.2 Exhaustive, with no default arm

Every case **MUST** be covered by exactly one arm, singly or inside one `[ ]` group. The block lowers to an O(1) runtime **tag jump**; if the scrutinee's static type is already a single case (`Expr.strLit`), that arm is chosen statically with no jump.

There is **no default or wildcard arm**. A catch-all carries no case information — it means only "every case not matched above" — and that set is implicit and unstable: adding a case to the variant, or editing another arm, silently changes it, so a newly added case could vanish into the default unhandled. Requiring every case to be named makes adding a case to the variant a compile-time error at every `match` until the case is placed. If several cases share an arm, name them in a `[ ]` group; there is no shorthand for "the rest."

> **Story:** [`stories/adt.md`](../stories/adt.md#variants-deserve-a-central-place) — "Variants deserve a central place".

### 5.3 Variant matching, not pattern matching

A `match` dispatches on a variant's **tag**, not on the **shape** of its payload. It selects which case is live and binds the payload whole. It does **not** reach into nested variants, destructure a `struct`, test literals, or apply guards — the operations ML-family *pattern matching* bundles together. To act on a nested case, `match` again on the payload. Bracket-grouping selects a **set of tags**; it is never a step toward matching shape.

> **Story:** [`stories/adt.md`](../stories/adt.md#variants-not-patterns) — "Variants, not patterns".

### 5.4 Abort flows through

If a chosen arm's body is abortable, the whole `match` is abortable and takes a `?` (or `??`) handler like any abortable expression. Abortability is not introduced or swallowed by `match`; it flows through.

```zane
result Int = match token {
    x number => parse(x);
    x symbol => lookup(x);
} ? msg {
    print(msg)
    resolve Int(20)
}
```

> **See also:** [`error-handling.md`](error-handling.md) §3.5 for the `?` integration.

### 5.5 Operations stay open

A variant's cases are fixed at its definition, but operations over it are open. Because every case is visible, any package may write its own function that matches an imported variant:

```zane
String render(e Expr) => match e { ... }
```

The type is closed; the operations are open.

### 5.6 Matching several scrutinees

A `match` may take **several scrutinees**, written as a bare comma-separated list — never parenthesised. Each arm then gives one selector per scrutinee position, in order and comma-separated, and the block dispatches on all the tags jointly.

```zane
type State = enum [ idle, running, paused ]
type Event = enum [ keyPress, timeout, reset ]

newState State = match state, event {
    [idle, running, paused], keyPress => State.running;
    running,                 timeout  => State.paused;
    [idle, paused],          timeout  => State.idle;
    [idle, running, paused], reset    => State.idle;
}
```

- **Bare list, not a tuple.** The scrutinees are independent values matched together, not a product value taken apart. Parenthesising them as `(state, event)` would imply a tuple to destructure — the pattern-matching road (§5.3) — so the list stays bare.
- **Per-position selectors.** Each arm carries one `[binder] selector` per scrutinee, in order. A `[ ]` group's commas sit inside its brackets and the position commas sit outside them, so the two never collide.
- **Cross-product exhaustiveness.** Every combination of cases **MUST** be covered, with no default. Because there is no wildcard, a "regardless of `state`" arm names every state in a `[ ]` group; adding a case to either variant is a compile-time error until the new combinations are placed.

> **Story:** [`stories/adt.md`](../stories/adt.md#variants-deserve-a-central-place) — "Variants deserve a central place".

---

## 6. Enum Maps

An enum map attaches **uniform external data** to an enum's members. It is a package-scope, exhaustive, **access-only** declaration — there is no value form, like a method — and it is read field-style with `.`.

```zane
Colors.colorName String [
    red = "Red",
    green = "Green",
    blue = "Blue"
]

Colors.red.colorName   // "Red" — a String value
```

The form is `<Enum>.<property> <VarType> [ member = value, ... ]`. It uses `[ ]` brackets with `,` separators, names the property where it is read, and reserves **no keyword**, so `Map` and `Dict` stay free for a future dictionary type.

- An enum map is **not a passable value**. The mapping is static, so there is nothing to dispatch over; only its *result* is a value. A genuinely dynamic `Colors → String` transform is a lambda (`String[Colors]`), not a map.
- Enum maps belong to enums specifically — uniform peers paired with uniform external data. A `variant` would never want one, because its data is intrinsic to each case.

> **Story:** [`stories/adt.md`](../stories/adt.md#payloadless-peers-keep-their-data-outside) — "Payloadless peers keep their data outside".

---

## 7. Language Comparisons

### 7.1 Difference summary

| Difference | Rust / OCaml | Zane |
|---|---|---|
| Enum vs. sum type | a single construct (`enum` / variant type) serves both payloadless constants and payload-carrying alternatives | two distinct constructs: `enum` for uniform peers, `variant` as the sum mould |
| Payloadless constants | a degenerate case of the sum type | a first-class `enum`, kept uniform and tabulatable |
| Per-member data | attached by writing a payload on a case | attached externally by an enum map, keeping members interchangeable |
| Matching model | **pattern matching**: an arm dispatches on the tag *and* destructures shape — nested patterns, tuple/record/literal patterns, guards | **variant matching**: an arm dispatches on the tag only and binds the payload whole; `match` again to go deeper (§5.3) |
| Match arms | dedicated pattern-match arm syntax | a central `match` block of `;`-terminated tag arms; a `[ ]` group shares one arm across several cases |
| Default arm | `_` wildcard absorbs unlisted cases | no default; every case is named, so adding a case is a compile error until placed (§5.2) |
| Multiple scrutinees | matched as a tuple pattern | a bare comma list of scrutinees, matched jointly on tags — no tuple, so no shape destructuring (§5.6) |

### 7.2 Zane vs. Rust

Rust's `enum` is a sum type: payloadless variants and payload-carrying variants are the same construct, and matching is pattern matching — arms destructure shape, nest, and guard.

**Rust:**
```rust
enum Color { Red, Green, Blue }            // payloadless
enum Expr { IntLit(String), Flip(Box<Expr>) }  // payload-carrying
```

**Zane:**
```zane
type Colors = enum [ red, green, blue ]    // uniform peers
type Expr = #variant { intLit String; flip &Expr; }   // recursive sum: reference type
```

| Difference | Rust | Zane |
|---|---|---|
| One keyword vs. two | `enum` covers both roles | `enum` for peers, `variant` for sums |
| Iteration / total tables | available, but mixed with payload cases | first-class on `enum`, whose members stay payloadless |
| Recursive boxing | `Box<T>` on the recursive field | explicit `&` on the recursive member |
| Match | pattern matching (destructures shape, nests, guards) | variant matching (tag only; no nesting or guards) |

> **Story:** [`stories/adt.md`](../stories/adt.md#two-constructs-against-the-hype) — "Two constructs, against the hype".

---

## 8. Summary

| Concept | Rule |
|---|---|
| `enum` | Closed set of lowercase, payloadless peer members in a `[ ]` list; accessed as `Type.member`; not a sum mould |
| `variant` | Sum mould; a value holds exactly one named member at a time; `{ }` body with `;`-terminated `member FieldType` entries, identical to a `struct` body |
| Variant construction | Built-in syntax `Variant.case(payload)` — not a constructor verb — names one case and yields the whole variant; an expression legal anywhere; the shorthand `e Variant.case(payload)` is the instantiation form and still declares `e` at the variant type; payloads compose by nesting, never by dotting through a case; a payloadless `enum` member is the argument-less degenerate; shares its `Type.member(args)` surface with a named constructor ([`types.md`](types.md) §3.4) but not its mechanism |
| Variant member read | Partial and therefore abortable; a single-payload case behaves as its payload once bound |
| struct/variant symmetry | One body grammar; the keyword flips meaning, construction, read totality, and layout; the `#` modifier picks value versus reference |
| `#variant` / `#enum` | `#variant` is the sum mould's reference form (identity, may recurse); `#enum` is a reference cell holding a tag; the `#` modifier applies uniformly |
| Recursion | Recursive members box through explicit `&`; a recursive type is a `#variant` or `#struct`, never a value type |
| Variant storage | `variant` is the sum mould's value form, laid out inline; `#variant` is its reference form, carrying a tag, may recurse, and is placed by the reference-type rules |
| `match` block | Expression legal in any expression position; `match scrutinee { [binder] selector => body; ... }`; one result type; runtime tag jump; static narrowing chooses statically; abort flows through with `?` |
| Match arm | `[binder] selector => body`; binder optional; selector is a case or a `[ ]` group of cases; a `[ ]` group is shorthand for one arm per case, each binding its own case's payload; the bracket is a selector, not a type; for the whole variant an arm reads the scrutinee |
| Exhaustiveness, no default | Every case covered by exactly one arm, singly or in a `[ ]` group; no wildcard, so adding a case is a compile error until placed |
| Variant matching, not pattern matching | `match` dispatches on the tag and binds the payload whole; no nested destructuring, guards, or shape tests |
| Multiple scrutinees | `match a, b { sel, sel => body; ... }`; bare comma list, never a tuple; one selector per position; cross-product exhaustiveness, no default |
| Open operations | The variant is closed; any package may match it in its own function |
| Enum map | Package-scope, exhaustive, access-only `<Enum>.<property> <VarType> [ member = value, ... ]`; read field-style; not a passable value |
