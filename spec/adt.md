# Zane Algebraic Data Types

This document specifies Zane's algebraic data types: the `enum` of uniform peer members, the `variant` sum type, the body symmetry that ties `variant` to `struct`, pattern matching via case-overload dispatch, the `match` expression, and enum maps.

> **See also:** [`types.md`](types.md) for value and reference types and constructors. [`syntax.md`](syntax.md) §1 and §4.8 for the surface forms. [`memory.md`](memory.md) §2.10 for why a value type cannot hold `&` or recurse. [`generics.md`](generics.md) §7 for the uniform-stride rule. [`functions.md`](functions.md) §4 and §5 for overload resolution. [`error-handling.md`](error-handling.md) §3.5 for `?` handlers on `match`. [`lexical.md`](lexical.md) §3 and §6 for casing and delimiters.

---

## 1. Overview

Zane separates two ideas that other languages often merge. An `enum` is a closed set of interchangeable, payloadless peers. A `variant` is a sum type whose members each carry a payload. Keeping them distinct lets each stay simple: enums are uniform and tabulatable, variants are heterogeneous and dispatched.

- **`enum is uniform peers`.** A closed set of lowercase, payloadless members that mean one uniform thing. Per-member data is attached externally by an enum map.
- **`variant is a sum type`.** A value holds exactly one of its named members. Its body grammar is byte-for-byte identical to a `struct`; the keyword flips product into sum.
- **`The # axis applies to sums`.** `variant` is a value sum; `#variant` is a reference sum (see [`types.md`](types.md) §2.1). The `#` mark applies the same way to an `enum` body.
- **`Reading a variant member is partial`.** A case may not be live, so a member read is abortable. The primary consumer is exhaustive dispatch.
- **`Dispatch is tag-directed and exhaustive`.** Case overloads (§5) and the `match` expression (§6) both lower a whole-variant value to a runtime tag jump that must cover every case.
- **`Recursion requires a reference type`.** A recursive sum must be a `#variant`, never a value `variant`, because a value type is transitively value and cannot hold the `&` a recursive member boxes through.

---

## 2. Enums

An `enum` is a closed set of **interchangeable peer** members that mean one uniform thing — colors, brands, weekdays. It is **not** a sum type. Its members are lowercase, payloadless peer values, written as a flat `[ ]` list.

```zane
type Operator = enum [ add, sub, mul, div, eq, lessEq, moreEq, less, more ]

type Colors = enum [ red, green, blue ]
```

A member is accessed as a value through the type name: `Colors.red`. The type is uppercase and the member is lowercase, so member selection (`Colors.red`) reads exactly like field selection (`vec.x`) and the casing rule (see [`lexical.md`](lexical.md) §3) is never broken.

### 2.1 Why an enum is not a variant

The property that distinguishes an `enum` is **uniformity** — the substitutability of intent — not structure. Enum members are peers you can iterate, ordinal-index, and build total tables over. The moment a case needs its own heterogeneous payload, it is a `variant`, not an `enum`.

Per-member associated data is attached externally through an enum map (§7), which keeps the members themselves payloadless and interchangeable. The consumers of an enum are iteration, ordinal use, total mapping, and exhaustive matching.

---

## 3. Variants

A `variant` is a sum type. A value holds **exactly one** of the variant's named members at a time. The body uses `{ }` brackets with `;`-terminated members, each a lowercase member name followed by its payload type — the same grammar a `struct` uses.

A plain `variant` is a **value** sum: copied on assignment, transitively value, non-recursive. A `#variant` is a **reference** sum: it has identity, may hold reference-type and `&` payloads, and may recurse (§4). A recursive sum such as `Expr` — whose members refer back to `Expr` through `&` — must therefore be a `#variant`:

```zane
type QualifiedIdent = tuple[String, String];
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

Reading a member of a variant value is **partial**: the case may not be the live one. A member read is therefore an **abortable** access (`?` / `??`, see [`error-handling.md`](error-handling.md)). The primary consumer of a variant is exhaustive dispatch (§5, §6). A single-payload case, once bound, behaves as its payload, so a value of `Expr.intLit`'s payload type reaches that payload's members directly.

### 3.1 The struct/variant symmetry

A `struct` and a `variant` share one declaration body. The keyword flips four things in lockstep. "Interchangeable" applies to the *declaration*, not to consuming code: construction and reads differ between the two.

| | `struct { a A; b B; }` (product) | `variant { a A; b B; }` (sum) |
|---|---|---|
| Meaning | has `a` **and** `b` | has `a` **or** `b` |
| Construct | must set **all** members | sets **exactly one** member |
| Read `e.a` | total — always an `A` | partial — abortable, an `A` only if `a` is live |
| Layout | sum of member sizes | tag + max of member sizes |

> **See also:** [`types.md`](types.md) §2.5 for the same relationship from the struct side.

---

## 4. Recursion and Storage

A directly inline self-reference would have infinite size, which the uniform-stride rule forbids (see [`generics.md`](generics.md) §7). A recursive member must therefore **box through `&`**: directly, as `flip &Expr`, or inside a named reference type it names, as `op BinOp` where `BinOp` holds `left &Expr; right &Expr;`.

- A value type — `struct` or `variant` — **cannot** hold an `&` or contain itself (see [`memory.md`](memory.md) §2.10). A recursive type must therefore be a reference type: a `#variant` or a `#struct`, **never** a value type. The body syntax is symmetric across all four kinds; the `#` modifier decides which may recurse.
- The `#` modifier is what carries recursion: a `variant` is an inline value sum, while a `#variant` is a reference sum that carries a tag, boxes its recursive cases through `&`, and is placed by the ordinary reference-type rules ([`memory.md`](memory.md) §3.5). A recursive sum such as `Expr` is a `#variant`.
- Indirection is always **explicit `&`**. There is no hidden auto-boxing, matching Zane's stance that ownership and refs are explicit.

---

## 5. Pattern Matching via Case-Overload Dispatch

A function may be overloaded on a variant's individual cases. The compiler enforces that the overloads cover every case.

```zane
String exprToString(x Expr.intLit)   => toString(x)
String exprToString(x Expr.floatLit) => ...
// one arm per case; all arms return String
```

This is **runtime tag dispatch**, distinct from the static overload resolution in [`functions.md`](functions.md) §5. A value whose static type is the whole variant lowers to a **tag jump table** over the case overloads.

- **One return type.** All arms share one return type, which is the result type of the call.
- **One-level unwrap.** A whole-variant value dispatches over its immediate cases. It does not recurse into nested variant payloads.
- **Static narrowing skips dispatch.** If the static type is already a case (`Expr.intLit`), the matching arm is chosen statically, with no runtime jump.
- **Exhaustiveness, checked per package.** All overloads of one function name over a variant's cases — the arms of one match — **MUST** be declared in the same package, and that package is where the compiler verifies every case is covered. An incomplete set is a compile-time error in that package.
- **Mutual exclusion.** For one function name, a whole-variant overload `f(x V)` and that variant's case overloads `f(x V.case)` **cannot coexist**. A call `f(value)` with `value : V` could not tell whether to match `V` directly or to unwrap and dispatch. The rule is symmetric — order does not matter — and overloads on unrelated types are unaffected.

This exhaustiveness rule is *per match set*, not per variant. A variant's cases are fixed at its definition, but operations stay open: any package may import a variant and declare its own complete match over every case, because the variant is closed and all its cases are visible. What is forbidden is *splitting one match* across packages — handling some cases in one package and the rest in another — because then no single compilation unit could verify completeness. (The inline `match` expression of §6 is a single local expression and is always checkable in place.)

> **See also:** [`functions.md`](functions.md) §4.4 for the same rule from the overload side.

---

## 6. The `match` Expression

`match` dispatches over a **collection of callables**. It takes a scrutinee and a flat `[ ]` list of function values, then dispatches on the variant's tag to the callable whose parameter type matches the live case. Each callable's parameter type identifies the case it handles, and the list must cover every case.

`match` has **no special arm syntax**. The arms are ordinary function values, so each may be a lambda literal or a lambda-variable (see [`functions.md`](functions.md) §7).

```zane
// lambda literals (=> expr is shorthand for { return expr })
result Int = match value [
    Int(x Num.int)   => 2 * x,
    Int(x Num.float) => Int(6 * x)
]

// lambda-variables — any callables of the right parameter types
result Int = match x [intCase, floatCase]
```

`Int(x Num.int) => 2 * x` is the expression-bodied lambda form of:

```zane
Int(x Num.int) {
    return 2 * x
}
```

- **Flat list.** The arms are written in `[ ]` with `,` separators — a value collection of function values (see [`lexical.md`](lexical.md) §6).
- **Expression.** `match` is an expression. All callables share one return type, which is the type of the `match`.
- **Exhaustive.** Every case is covered by exactly one callable (or a wildcard).
- **Abort flows through.** `match` passes the chosen callable's output one level up. If the callables are abortable, the whole `match` is abortable and takes a `?` handler like any abortable expression.

```zane
result Int = match x [intCase, floatCase] ? msg {
    print(msg)
    resolve Int(20)
}
```

The two dispatch surfaces relate directly. Case-overload dispatch (§5) dispatches via *named overloaded functions*, which have no value form. `match` dispatches via an *explicit collection of function values*. Both use the same tag dispatch and the same exhaustiveness requirement.

> **See also:** [`error-handling.md`](error-handling.md) §3.5 for the `?` integration.

---

## 7. Enum Maps

An enum map attaches **uniform external data** to an enum's members. It is a package-scope, exhaustive, **access-only** declaration — there is no value form, like a method — and it is read field-style with `.`.

```zane
Colors.colorName String [
    red = "Red",
    green = "Green",
    blue = "Blue"
]

Colors.red.colorName   // "Red" — a String value
```

The form is `<Enum>.<property> <Type> [ member = value, ... ]`. It uses `[ ]` brackets with `,` separators, names the property where it is read, and reserves **no keyword**, so `Map` and `Dict` stay free for a future dictionary type.

- An enum map is **not a passable value**. The mapping is static, so there is nothing to dispatch over; only its *result* is a value. A genuinely dynamic `Colors → String` transform is a lambda (`String[Colors]`), not a map.
- Enum maps belong to enums specifically — uniform peers paired with uniform external data. A `variant` would never want one, because its data is intrinsic to each case.

---

## 8. Language Comparisons

### 8.1 Difference summary

| Difference | Rust / OCaml | Zane |
|---|---|---|
| Enum vs. sum type | a single construct (`enum` / variant type) serves both payloadless constants and payload-carrying alternatives | two distinct constructs: `enum` for uniform peers, `variant` for sum types |
| Payloadless constants | a degenerate case of the sum type | a first-class `enum`, kept uniform and tabulatable |
| Per-member data | attached by writing a payload on a case | attached externally by an enum map, keeping members interchangeable |
| Match arms | dedicated pattern-match arm syntax | ordinary function values — case overloads (§5) or a `match` callable list (§6) |

### 8.2 Zane vs. Rust

Rust's `enum` is a sum type: payloadless variants and payload-carrying variants are the same construct, and matching uses dedicated arm syntax.

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

---

## 9. Design Rationale

| Decision | Rationale |
|---|---|
| `enum` and `variant` are distinct constructs | Uniform peers and heterogeneous sums are different ideas. Keeping them separate lets enums stay iterable and tabulatable while variants carry per-case payloads, instead of overloading one keyword with both roles. |
| `variant` shares the `struct` body grammar | One body shape with the keyword flipping product into sum keeps the surface minimal and makes the and/or duality obvious at a glance. |
| Variant member reads are abortable | A case may not be live, so a partial read is naturally a bifurcated return rather than a silent failure or a sentinel. |
| Dispatch is tag-directed and exhaustive | Closing the case set lets the compiler guarantee every case is handled and lower a whole-variant value to an O(1) tag jump. |
| Exhaustiveness is checked per package | A package compiles as one unit, so requiring a match's arms to be co-located lets the compiler verify completeness locally and early — while still letting any package add its own complete match over an imported variant, keeping the type closed but operations open. |
| Mutual exclusion of whole-variant and case overloads | A call on a whole-variant value would otherwise be ambiguous between matching the variant directly and unwrapping into a case; forbidding the coexistence removes the ambiguity by construction. |
| `match` arms are ordinary callables | Reusing lambda literals and lambda-variables avoids inventing special arm syntax and lets arms be named, reused, and passed like any other function value. |
| Recursion boxes through explicit `&` | Inline self-reference breaks uniform stride; explicit `&` keeps indirection visible and consistent with Zane's explicit-ownership stance, with no hidden auto-boxing. |
| A value type can never be recursive | Value types are closed inline storage that cannot hold `&`; a recursive shape must therefore be a reference type — a `#variant` or `#struct` — which the symmetric body grammar still permits. |
| Enum maps instead of per-member payloads | Attaching uniform data externally keeps enum members payloadless and interchangeable, and naming the property at the read site needs no new keyword. |
| Enum maps are access-only, not values | The mapping is static, so there is nothing to dispatch over; a dynamic transform is just a lambda. |
| No interfaces or constraints | If two enums travel together constantly, that is evidence they are one enum and should be merged. If they don't, wrapping them in a `variant` plus a few matching functions is a fair price for a rare case. Building open-extension machinery for a situation that resolves itself is overengineering. |

---

## 10. Summary

| Concept | Rule |
|---|---|
| `enum` | Closed set of lowercase, payloadless peer members in a `[ ]` list; accessed as `Type.member`; not a sum type |
| `variant` | Sum type holding exactly one named member at a time; `{ }` body with `;`-terminated `member FieldType` entries, identical to a `struct` body |
| Variant member read | Partial and therefore abortable; a single-payload case behaves as its payload once bound |
| struct/variant symmetry | One body grammar; the keyword flips meaning, construction, read totality, and layout; the `#` modifier picks value versus reference |
| `#variant` / `#enum` | `#variant` is a reference sum (identity, may recurse); `#enum` is a reference cell holding a tag; the `#` modifier applies uniformly |
| Recursion | Recursive members box through explicit `&`; a recursive type is a `#variant` or `#struct`, never a value type |
| Variant storage | `variant` is an inline value sum; `#variant` is a reference sum that carries a tag, may recurse, and is placed by the reference-type rules |
| Case-overload dispatch | Overload a function on a variant's cases; runtime tag jump, one return type, one-level unwrap, exhaustive; static narrowing chooses statically |
| Match exhaustiveness | The arms of one match (one function's case overloads over a variant) must all live in one package, where completeness is verified; the type is closed but operations stay open |
| Whole-variant vs. case mutual exclusion | `f(x V)` and `f(x V.case)` cannot coexist for one function name |
| `match` | Expression dispatching a scrutinee over a `[ ]` list of callables; exhaustive; one return type; abort flows through with `?` |
| Enum map | Package-scope, exhaustive, access-only `<Enum>.<property> <Type> [ member = value, ... ]`; read field-style; not a passable value |
