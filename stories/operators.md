# Stories: Operators

> **See also:** [`spec/operators.md`](../spec/operators.md) — the rules these chapters explain.

## A small vocabulary worth overloading

Operators posed two questions that are easy to conflate: whether programmers could change what familiar notation means for their own types, and whether they could invent new notation altogether. We wanted the first and rejected the second. Refusing overloading, as Go does, would keep the language surface exceptionally plain, but it would also make every user-defined numeric, geometric, or collection type a second-class citizen. A vector that cannot use the same addition notation as a scalar has lost much of what operators are for. Overloading the built-in set was therefore essential.

That did not imply an open-ended operator language. Custom tokens bring custom precedence, or at least the question of it, and make a reader learn a local dialect before an expression can be grouped. We instead kept one small, fixed vocabulary whose implementations may vary while its grammar does not. This is a deliberate middle ground: abstractions may participate in ordinary notation, but they may not extend the notation itself. The cost is that a domain whose natural operation has no place in the fixed set must use a named verb, even where a bespoke symbol would be concise.

Overloading also exposed a broader question. An overloaded name is easy to resolve at a call, where the operands and their types are present, but ambiguous when passed as a value to another overloaded verb. We initially met that as an obstacle specific to functions. Operators showed that it was really a lapse in orthogonality: they work cleanly because an operator token is call-only and can never be detached from its operands. We applied the same rule to every verb. A programmer who wants behavior as a value writes a lambda, which is an ordinary expression and can be stored or passed without asking an overloaded name to choose an implementation in a vacuum. That gives up the convenience of passing a bare function or operator name, but it gives all verbs one resolution model instead of adding special rules for each callable form.

## One operator for flipping a value

The unary vocabulary became constrained before we chose its spelling. `!` had already been reserved to make mutating method calls visible, so it could not also mean boolean negation. At the same time, a preference for orthogonality made separate unary `-` and boolean-NOT operations look wasteful. Additive inverse and logical complement perform the same higher-level act: they **flip** a value. That idea extends beyond numbers and booleans; vectors and other composite values can define what their own flip means without requiring another unary concept.

We still needed a symbol for it. Unary `-` was familiar for numbers, but reusing the subtraction token would obscure the fact that this was a universal operation and would produce the jarring `-true` for booleans. `!` was already occupied, and using it in the derived spelling of subtraction would make that operation look alien. `~` resembled `-` closely enough to suggest reversal while remaining visibly distinct, and Lua's established `~=` spelling made the resulting inequality notation feel less invented. So `~` became the sole unary operator: logical complement for booleans, additive inverse for numbers, and the general flip point for types that define one.

The price is immediate familiarity. Readers coming from most C-family languages will first read `~value` as bitwise complement, and `~true` is less conventional than `!true`. Zane spends that familiarity to keep the unary model small and to leave `!` with one unambiguous job.

## Deriving the laws instead of trusting them

Once flip was primitive, several other operators no longer needed independent definitions. Subtraction could be addition of a flipped value; inequality could be flipped equality; and three comparisons could be expressed through `<` and flip. Letting a type implement those spellings separately would add flexibility only by permitting it to lie—for example, to make `a > b` disagree with `b < a`. Where a derivation is trivial, we preferred to make the law inexpressible to violate. The smaller implementation surface is useful, but it is the consequence rather than the point. It also rewards a type for defining the broadly useful `~`: one implementation unlocks several derived forms.

Division is the deliberate boundary. Deriving `a / b` as `a * (1 / b)` assumes a multiplicative inverse exists, and floating-point arithmetic makes even the apparently equivalent reciprocal-multiply transformation observably different. Compilers place such transformations behind relaxed floating-point semantics for exactly that reason. `/` therefore remains primitive: a type can implement division directly without pretending it is multiplication in disguise.

Derivation has a cost of its own. A domain cannot provide an efficient specialized `>` or `-` whose behavior merely agrees with the law; the fixed expansion is the only spelling the language recognizes. We accept that lost optimization hook because an operator law that tools and readers can rely on is more valuable than an extra implementation point.

## Grouping is grammar all the way down

Allowing implementations to vary made it more important that grouping never does. Precedence belongs to the grammar, not to types or declarations, so changing an operand type cannot reshape the expression around it. `and` and `or` remain keywords because their short-circuit behavior is control flow: an ordinary overloaded verb would receive already evaluated operands and could not preserve it.

The same orthogonality settles comparison chains. Python gives `a < b < c` a special chained-comparison meaning. We did not adopt that exception, nor did we prohibit the form to protect programmers from C-like surprises. Comparisons group left like the other binary operators, so the expression means `(a < b) < c`. Usually that second `<` has no matching overload and the program is rejected; if a package deliberately defines `<` for the boolean result and the third operand's type, the expression is valid. The grammar does not decide that one sequence of operands deserves a special interpretation.

This rule permits code that many readers will initially assume is either forbidden or Python-like, and a deliberately unusual boolean comparison overload can make it meaningful. That is the honest cost of total regularity here. Parentheses remain the way to communicate an intended grouping when the conventional reading would be uncertain.

## Imports may add names, not meanings

The last pressure came from separate compilation and ordinary maintenance. If any imported helper package could contribute an operator overload, adding or removing an import could silently change the meaning of an existing `a + b`. We wanted edits to imports to affect the names a file may mention, not reinterpret expressions that were already valid.

Operator implementations therefore belong to the home package of at least one operand type. A unary operator lives with its operand; a binary operator lives with either side. Resolution can find the candidates from the types already present in the expression, without treating the import list as an invisible extension registry. This follows the same coherence instinct as home-package methods, but operators make the danger sharper because their names are punctuation and the declaring package never appears at the call site.

The restriction closes off retroactive adaptation in a neutral third package. When two foreign types ought to interoperate, their operators cannot be supplied by an unrelated bridge package; one of the home packages must own the integration, or the caller must use a named conversion or verb. We take that friction over allowing a seemingly harmless import to rewrite the meaning of existing notation.
