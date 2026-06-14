# Zane ADT & Delimiter Proposal (for review)

This is a **scratch proposal**, not a spec document. It records every decision from the algebraic-data-types discussion so it can be verified before any `spec/` files change. Delete or fold it once the design is confirmed.

Status tags: **[DECIDED]** = settled in discussion; **[OPEN]** = still needs a call.

> The type-system redesign (PR #69) is merged into `main`. This ADT work continues on a fresh branch.

---

## 0. Open questions

Both prior open items are now resolved:

1. **`match` shape — RESOLVED.** Arms are ordinary callables (full lambdas or lambda-variables), not special syntax. See §7.
2. **`map` syntax — RESOLVED.** Keyword-less property-table form approved. See §8.

Everything below is **[DECIDED]**. Please confirm it matches your intent before I write the spec.

---

## 1. `enum` — uniform peer constants [DECIDED]

An `enum` is a closed set of **interchangeable peer** members that mean one uniform thing (colors, brands, weekdays). It is **not** a sum type.

- Brackets `[ ]`, `,`-separated (it's a flat list).
- Members are **lowercase peer values**, **payloadless**.
- Accessed as a value: `Colors.red`.

```zane
type Operator = enum [ add, sub, mul, div, eq, lessEq, moreEq, less, more ]

type Colors = enum [ red, green, blue ]
```

**Why enum ≠ variant:** the distinguishing property is *uniformity / substitutability of intent*, not structure. Enum members are peers you can iterate, ordinal-index, and build total tables over. Rust/OCaml collapse enums into sum types and lose that distinction; Zane keeps them separate on purpose. The moment a case needs its *own* heterogeneous payload, it's a `variant`, not an `enum`. Per-member associated data is attached externally via a map (§8), which keeps members payloadless and interchangeable.

Consumption: iteration / ordinal / total mapping, and exhaustive matching (`f(x Colors.red)`, §6).

---

## 2. `variant` — sum type, identical body to `struct` [DECIDED]

A `variant` is a sum type whose body grammar is **byte-for-byte identical to a `struct`**. The keyword alone flips product → sum.

- Brackets `{ }`, `;`-separated members (same as struct, §5).
- Cases are **lowercase member names** (like struct fields), each with a payload type.
- A case projected as a type is `Expr.intLit` — `Expr` is the type (uppercase), `.intLit` is member selection (lowercase), exactly like `vec.x`. So nothing in the casing rule breaks.

```zane
type Expr = variant {
    intLit String;
    floatLit String;
    strLit String;
    boolLit Bool;
    ident String;
    qualifiedIdent tuple[String, String];
    op struct { left &Expr; right &Expr; operator Operator };
    flip &Expr;
    parenthesized &Expr;
    funcCall FuncCall;
    funcLambda FuncLambda;
    methLambda MethLambda;
}
```

Reading a member of a variant is **partial** (the case may not be live), so it is an **abortable** access (`?`/`??`). The primary consumer is exhaustive dispatch (§6). A single-payload case, once bound, behaves as its payload (so `color.colorName` reaches the payload's members).

---

## 3. struct ≡ variant symmetry [DECIDED]

Same declaration body; the keyword flips four things in lockstep. "Interchangeable" applies to the *declaration*, not to consuming code.

| | `struct { a A; b B }` (product) | `variant { a A; b B }` (sum) |
|---|---|---|
| Meaning | has `a` **and** `b` | has `a` **or** `b` |
| Construct | must set **all** members | sets **exactly one** member |
| Read `e.a` | total — always an `A` | partial — abortable, an `A` only if `a` is live |
| Layout | sum of member sizes | tag + max of member sizes |

---

## 4. Recursion & memory boundary [DECIDED]

- A directly-inline self-reference is infinite size, forbidden by the uniform-stride rule (generics.md §7). **Recursive members box through `&`:** `flip &Expr`, `op struct { left &Expr; … }`.
- **Structs cannot hold `&` or contain themselves** (memory.md §2.10). Therefore a recursive type can be a `variant` or a `class`, but **never a `struct`**. The body syntax is symmetric; the memory model decides which forms are legal.
- A `variant` follows the same storage split as `struct`/`class`: inline value type when every payload is inline-safe and non-recursive; **owned heap type otherwise**. Recursive variants (like `Expr`) are owned, get a tag, and box recursive cases.
- Indirection is **explicit `&`** — no hidden auto-boxing, matching Zane's "ownership and refs are explicit" stance.

---

## 5. Delimiters [DECIDED — option A]

The delimiter follows *what is being separated*:

- **`;`** separates **members of a declaration body**: `struct` / `class` / `variant`. Used **always**, inline or multiline — newlines are **insignificant inside these bodies** (option A).
- **`,`** separates **elements of a value collection**: array literals, `tuple`, `enum`, call/constructor args, `init{}` fields, generic args, and `match` arm lists.
- **newline** separates **statements** (function bodies, control-flow blocks). No statement separator exists, so two statements can't share a line.

Bracket convention:

- **`{ }`** = named-typed-member body (`struct`/`class`/`variant`) and code/init blocks.
- **`[ ]`** = flat list (array, `tuple`, `enum`, `match` arms, function-type param lists).

Consequence to accept: newlines are structural for statements but not inside `;`/`,` collections; the parser always knows which context it's in (type-expression body vs code block). Zane introduces no `;` on statements.

```zane
type Color = struct { r Int; g Int; b Int }    // inline body, ';'

Void main() {
    x Int(5)                                     // statements, newline
    print(x)
}
```

---

## 6. Dispatch — pattern matching via overloads [DECIDED]

Overload a function on a variant's cases; the compiler enforces exhaustiveness.

```zane
String exprToString(x Expr.intLit)   => toString(x)
String exprToString(x Expr.floatLit) => ...
// one arm per case; all return String
```

- This is **runtime tag dispatch**, distinct from the static overload resolution in functions.md §5: a value whose static type is the whole variant lowers to a **tag jump table** over the arms.
- All arms share **one return type** (the result type of the call).
- **One-level unwrap**: a whole-variant value dispatches over its immediate cases; it does not recurse into nested variant payloads.
- **Static narrowing skips dispatch**: if the static type is already a case (`Expr.intLit`), the matching arm is chosen statically.
- **Mutual exclusion rule:** for one function name, a whole-variant overload `f(x V)` and that variant's case overloads `f(x V.case)` **cannot coexist** — a call `f(value)` with `value : V` couldn't tell whether to match `V` directly or unwrap-and-dispatch. Overloads on unrelated types are unaffected. The rule is symmetric (order doesn't matter).
- Exhaustiveness requires all arms to be visible (the set is closed).

---

## 7. `match` — dispatch over a collection of callables [DECIDED]

`match` has **no special arm syntax**. It takes a scrutinee and a **collection of callables**, and dispatches on the variant's tag to the callable whose parameter type matches the live case. Each callable's parameter type identifies the case it handles, and the collection must cover every case (exhaustive).

The arms are ordinary function values, so they can be **lambda literals** or **lambda-variables**:

```zane
// lambda literals (=> expr is shorthand for { return expr })
result Int = match value [
    Int(x Num.int)   => 2 * x,
    Int(x Num.float) => Int(6 * x),
]

// lambda-variables — any callables of the right parameter types
result Int = match x [intCase, floatCase]
```

`Int(x Num.int) => 2 * x` is simply the expression-bodied lambda form of:

```zane
Int(x Num.int) {
    return 2 * x
}
```

- Brackets `[ ]`, `,`-separated — a flat list of function values (§5).
- `match` is an **expression**; all callables share one return type, which is the type of the `match`.
- **Exhaustive** — every case covered by exactly one callable (or a wildcard).
- **Abort flows through.** `match` just passes the chosen callable's output one level up, so if the callables are abortable, the whole `match` is abortable and takes a `?` handler like any abortable expression:

```zane
result Int = match x [intCase, floatCase] ? msg {
    print(msg)
    resolve Int(20)
}
```

Relationship to §6: §6 dispatches via *named overloaded functions* (which have no value form); §7 dispatches via an *explicit collection of function values*. Same tag-dispatch + exhaustiveness, two surfaces. Cross-references error-handling.md for the `?` integration.

---

## 8. `map` — enum associated property [DECIDED]

A map attaches **uniform external data** to an enum's members. It is a package-scope, exhaustive, **access-only** declaration (no value form, like a method per glossary §3.8). It is read field-style with `.`:

```zane
Colors.colorName String [
    red = "Red",
    green = "Green",
    blue = "Blue",
]

Colors.red.colorName   // "Red" — a String value
```

Form: `<Enum>.<property> <Type> [ member = value, … ]`. Uses `[ ]` + `,`, names the property where it's read, and reserves **no keyword** — so `Map`/`Dict` stays free for a future dictionary type.

- It is **not a passable value** — the mapping is static, so there is nothing to dispatch over. Its *result* is a value. If a genuinely dynamic `Colors → String` transform is ever needed, that's a lambda (`String[Colors]`), not a map.
- Belongs to enums specifically (uniform peers + uniform external data). A variant would never want it; its data is intrinsic.

---

## 9. Constructor key-value uses `=`, not `:` [DECIDED — NEW]

Named construction switches from `:` to `=`, matching `type X = …`, enum/map tables, and declarations generally. Assignment is not an expression, so `name = value` in an argument or `init` position is unambiguously a named field.

```zane
// init{ }
Vector{x Int, y Int} {
    return init{ x = x, y = y }   // was: x: x, y: y
}

// field-constructor call
starter Weapon{ fireRate = Float(2) }   // was: fireRate: Float(2)

// named constructor args
p Color(r = x, g = Int(0), b = Int(0))
```

Bare-field shorthand is unchanged: `init{ x, y }` still means `init{ x = x, y = y }`, and `Vector{ x, y }` still means `Vector{ x = x, y = y }`.

This touches existing examples in `types.md` and `syntax.md` (all `init{}`, field-constructor, and named-argument sites).

---

## 10. Deliberately omitted: interfaces / constraints [DECIDED]

Zane will **not** add structural or nominal interfaces for "any type with property X." Rationale to record verbatim:

> If two enums travel together constantly, that is evidence they are one enum and should be merged. If they don't, wrapping them in a `variant` plus a few matching functions is a fair price for a rare case. Building open-extension machinery for a situation that resolves itself is overengineering.

Cross-type uniform behavior is handled by merging enums or by a `variant` + closed dispatch (§6), accepting per-arm duplication when arms happen to be identical.

---

## 11. Files this will touch (when approved)

- **`spec/adt.md`** (new) — §1 enum, §2 variant, §3 symmetry, §4 recursion boundary, §6 dispatch, §7 match, §8 map. Design-rationale rows incl. §10 omission.
- **`spec/lexical.md`** — §5 delimiters (`;` / `,` / newline) and the `{}` / `[]` convention.
- **`spec/syntax.md`** — grammar for `enum` / `variant` / map declarations, `match`, the delimiter forms, and the constructor `=` change.
- **`spec/types.md`** — struct↔variant relationship; constructor `:` → `=` throughout.
- **`spec/functions.md`** — `match` as a callable-collection dispatch; relationship to overload resolution (§5).
- **`spec/error-handling.md`** — `match` is abort-transparent and accepts a `?` handler.
- **`spec/glossary.md`** — new terms (sum/product symmetry, case dispatch, enum vs variant, map property).
- **`README.md`** — add the `adt.md` row.
