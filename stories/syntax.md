# Stories: Syntax

> **See also:** [`spec/syntax.md`](../spec/syntax.md) — the forms these chapters explain.

## The order assignment forced

Every declaration in Zane names its type, and the language infers nothing about it. That is the commitment the rest of this chapter is downstream of, so it is worth stating the case against inference plainly rather than assuming it. The argument *for* inference is that the type is already determined — `number = 2` fixes an integer whether or not anyone writes `Int` — so writing it again is redundant, and a compiler that can recover the information should. What that argument quietly assumes is that a declaration's job is to inform the compiler. It is not; the compiler was never the reader in difficulty. A declaration's job is to say, at the place a reader looks, what this name now holds. Calling the type "excess information" is a claim about the compiler's needs dressed up as a claim about the source, and the trade it actually makes is to delete the one token that answers the reader's question in exchange for a saved keystroke. So types are written, everywhere, and the [captured-intent bet](foundations.md#the-bet-on-captured-intent) is the reason: the source states what is meant, and the compiler is not the party being spared.

Once the type is mandatory, the question is where on the line it goes, and it looks at first like a coin flip. We went looking for a language that had already answered it without falling back on inference, and found one: C++, whose construction form writes `Int number(2)` — the type, the name, the constructor arguments, no `=` and nothing inferred. It is a genuinely elegant line, and for a while it was the model. The type leads, which suits a reader scanning a column of declarations for a shape, and the constructor arguments ride along in parentheses with no assignment operator in sight.

What decided against it was not the declaration. It was every *other* place in a program where a type meets a value, and the sharpest of those is overwriting a field. Carry the same shape over and a field write reads:

```zane
player.health(20)
```

Three things are wrong at once, and each is worse than the last. The line is ambiguous with a method call — `player.health(20)` is exactly what invoking a `health` method on `player` would look like, and nothing in the text separates the two. The type is not written at all, so the explicitness we had just refused inference to preserve has gone missing at precisely the site where a reader most wants it. And nothing in the line signals a store; an overwrite is a consequential act and it reads here like an expression evaluated for its own sake.

So a field write has to name its type and mark itself as a store: `player.health = Int(20)`. Which raises the question the whole chapter turns on — if the C++ shape works for a declaration and not for a write, what is different? Put the two forms side by side and it is not subtle:

```zane
Int number(2);
player.health = Int(20);
```

The type is on the left in one and on the right in the other. These are the same act — produce a value of some type, put it in a named place — written in two directions. The tell is what happens when you try to repair the declaration instead of the write. Move the construction to the side the write puts it on and the declaration becomes:

```zane
Int(2) number;
```

and the conflict is now in plain sight. `Int` cannot be both the type annotation on the left and the constructor call on the right; it is one token doing a job that has two sides to it. Assignment settles which side wins, because assignment's direction is not ours to choose — the value comes from the right and lands on the left, in every language anyone has written and in every line of arithmetic anyone learned before that. The constructor is a value-producing expression, so it belongs on the right, and the place being written belongs on the left. Everything else follows from that one fixed point.

What follows is that the name leads. A declaration writes its symbol, then its type, then constructs:

```zane
number Int(2);
player.health = Int(20);
```

and the short form is [shorthand for the long one](https://github.com/zane-lang/spec/blob/94d3833d094932802dfd96419db1b067e1271e03/spec/syntax.md#11-symbols) — `number Int(2)` means `number Int = Int(2)`, which is the field write with a type annotation in front of the destination. The two lines are now the same line with and without a declaration on it. `Int` appears once as a type and once as a constructor, in that order, in both; the shorthand's whole content is that when those two are the same name you may write it once.

The cost is a token, and it is fair to name it rather than pretend the shorthand is free. Written out, `number Int = Int(2)` says `Int` twice, and the reason the language offers a shorthand at all is that the doubling is real and would otherwise be paid on every line. We are buying explicitness with repetition, collapsing the repetition where we can, and still paying it at every field write — `player.health = Int(20)` names the type each time, forever, where a language with inference writes `player.health = 20`. That is the bill for refusing inference and we do not think it can be argued away; the position is that the token is worth what it says.

The second cost is subtler and it is a debt owed to a different document. `number Int(2)` is two identifiers with nothing between them, and it is only readable because the second one is obviously a type — which it is only because [casing carries the kind](lexical.md#casing-carries-the-kind). Written in a language where `number int(2)` were legal, this form would be unreadable mush. The order we chose is affordable because of a rule that was decided on other grounds, and it would not survive that rule being relaxed.

## One shape, and everywhere it turned up

Having settled the order for one kind of declaration, we did not go on to design forms for the others. The shape `name Type(args)` turned out to fit them already, which is the sort of thing that reads as design and mostly was not; the forms met because each was the same act underneath — name a place, say what it holds, build what goes in it.

[Named constructors](https://github.com/zane-lang/spec/blob/a3760ba3179eb827acb0eefd7b885504e7db29e6/spec/types.md#34-named-constructors) and variant cases slot in first: the type position accepts a qualified `Type.member`, so `v Vector2.diagonal(Float(3))` and `e Expr.intLit("5")` are the one form with a longer name in the type slot, and the [types story](types.md#named-constructors-and-the-syntax-variants-already-had) tells how those two arrived at one spelling from opposite directions. A package constant is the same line at package scope. A defaulted entry in a [field-constructor header](https://github.com/zane-lang/spec/blob/94d3833d094932802dfd96419db1b067e1271e03/spec/syntax.md#34-field-constructors) is written `name String("Pistol")` — a constructor parameter with a default, spelled exactly like the declaration it will initialize, because it *is* that declaration form reused. And a [lambda variable](https://github.com/zane-lang/spec/blob/94d3833d094932802dfd96419db1b067e1271e03/spec/syntax.md#38-lambda-literals-and-lambda-variable-declarations) is the same line again with a function type in it, which the next chapter takes up on its own, because that one did not fall out — it was rejected somewhere else first and arrived here by a longer road.

There is one seam in all this reuse and it is worth pointing at, because it is the kind of thing a reader trips over once and remembers with irritation. A bare `name Type`, with no construction after it, is not a declaration — symbols require [direct initialization](https://github.com/zane-lang/spec/blob/4026aba01b3e07b28cf0476b79e32a354f8c22c3/spec/memory.md#211-symbols-require-direct-initialization) and the bare form is illegal as a statement. But inside a field-constructor header the very same text is legal and means something else: a required input the caller must supply. So the same two tokens are an error in one brace and a parameter in another, and the only thing distinguishing them is which body they sit in. We accept it because a required constructor input genuinely has no value to write — that is what "required" means — but it is a real wrinkle, and the alternative of minting a separate marker for it would have spent grammar to make a rule that context already settles.

## Two orders, and the one we had already turned down

The shape does not reach the one declaration a reader meets most often. A named verb puts its type first, exactly as the C form this story opened by rejecting does:

```zane
Unit shoot(this Player) mut { ... }
callback Unit(this Player) mut { ... }
```

Both of those are legal, they sit in the same files, and they are in opposite orders. That looks like the inconsistency the first chapter set out to remove, so it is worth saying what happened, including the part where we tried it the other way and did not like it.

The C order was there first — the language used it from the beginning, before any of the reasoning about declarations had been done — and when that reasoning arrived we did go back and try the consistent thing. `add Int(a Int, b Int)` is a perfectly writable form and we lived with it for a while. What was wrong with it is that it detaches the name from the definition. `Int add(a Int, b Int)` reads as one phrase — a thing that takes these and gives back an `Int` — with the name sitting inside the phrase where it belongs. `add Int(a Int, b Int)` reads as two things: a name, and then a specification attached to it. The definition stops being a sentence about what the function does and becomes a label with a signature bolted on. We should be honest that this is a judgement about how a line reads and not a derivation from anything; nobody can be shown to be wrong for preferring the other one. It is an aesthetic call, we made it, and we kept it.

What makes the episode worth recording is that the rejected form was not wasted. It was waiting for a job we had not yet created. Overloading means a bare callable name denotes a *set* rather than a value, so a function name cannot be an expression — the [functions story](functions.md#names-that-are-not-values) tells why that had to be true — and the moment it became true, being able to make a function *value* stopped being a convenience and became load-bearing, because callbacks are real and something has to carry them. A lambda is that value; a symbol holding one is a lambda variable. And a symbol holding a value is precisely what the first chapter settled the order for. So:

```zane
add Int(a Int, b Int) { ... }
add Int[Int, Int] = Int(a Int, b Int) { ... }
```

The second line is what the first one means, and it is the declaration shorthand with a function type where `Int` would be and a literal where the constructor call would be. The form we turned down for a named verb turned out to be exactly right one step over.

Seen from here the two orders stop being an inconsistency and start being a distinction the surface is drawing. A return-type-first line **defines a verb**. A name-first line **declares a symbol**, and its type happens to be a function. Those are different acts — one adds a callable to a package, the other puts a value in a named place — and a reader can tell which is which at a glance, by the same signal that separates a declaration from a definition everywhere else in the file. The two forms differ by one token swapped, which is either elegant or a trap depending on your mood; what makes it the former is that the swap tracks a real difference rather than decorating one.

That expansion also settles a bracket. `Int[Int, Int]` is the *type*, `Int(a Int, b Int) { ... }` the *literal*, and they cannot both use `( )` — written the same way, `callback Unit(this Player) mut` would be a symbol of function type and a lambda variable missing its body, with nothing in the text to say which. So the type takes `[ ]` and the literal keeps `( )`; the choice of which one moves is settled by the [bracket rule](lexical.md#the-bracket-picks-the-separator), under which a flat `,`-separated list is `[ ]` territory. There is no `->` for the same reason there is no `var`: the return type already leads, so an arrow would mark a direction the word order has stated, and it would spend width on the line least able to afford it.

One verb declaration wears neither order:

```zane
(this CustomList)[index Int] => this._data[index]
```

A [subscript](https://github.com/zane-lang/spec/blob/a3760ba3179eb827acb0eefd7b885504e7db29e6/spec/functions.md#29-subscripts-are-place-projections) is a method — it takes a subject and it reaches that subject's private fields like one — so written as an ordinary method its parameters would sit together, `(this CustomList, index Int) => ...`. What pulls them apart is the call site. A subscript is invoked as `list[i]`, with the indices in a `[ ]` that trails the subject, and the declaration splits its parameters to match: subject in `( )`, indices in the trailing `[ ]`. The declaration is shaped like the call it answers. No return type is written because there is none to write — the body must be a place expression, so the result type is whatever the projected place already is, and declaring it would restate a field type the container has already fixed.

That is the honest account and it is thinner than the rest of this file, so it should be labelled as such: subscripts have not been revisited since they were first written down. Nothing has pushed on them, no alternative was weighed at length, and the shape above is a first draft that has survived by not being tested rather than by winning anything. If a later pass finds a better form it will not be overturning an argument, because there is not much of one here to overturn.

## Words are paid for by the line

The other standing pressure on Zane's surface is width. This is not the familiar claim that keywords are bad; keywords are fine, and the language keeps the ones that earn it. The claim is narrower and more mechanical: a signature is where a language spends its horizontal budget, and a Zane signature is already carrying a return type, an abort type, a subject, parameter names with their types, and possibly a `mut`. Every word added to that line is width spent on grammar rather than on what the code is about, and past some length a signature stops being read and starts being skimmed. So words are bought, not assumed.

That is why there is no `var` or `let` in front of a declaration, and no `func` or `def` in front of a function. Neither would tell a reader anything the line does not already say — a name followed by a type followed by arguments is a declaration whatever word precedes it, and a return type followed by a name followed by a parameter list is a function. A word whose whole content is to announce the construction it precedes is a word the construction announces itself.

The same reasoning retires the `:` that Pascal, ML, TypeScript, Rust, and most of their descendants put between a name and its type. Its job is to signal that what follows is a type, and it is a perfectly good signal in a language where `name type` and `name value` are indistinguishable strings. In Zane they are not: the casing rule means a type's first letter has already given the answer by the time the `:` would arrive. The separator carries no information anything else does not already carry, so it goes. What Zane does keep is every word that names an act nothing else names — `type`, `alias`, `import`, `return`, `match`, `spawn`, `implicit`, `mut`, `this` — and it went as far as removing `if` and `while` from the grammar entirely, which the [control-flow story](control-flow.md#doing-without-while) tells as its own episode. The budget is not zero. It is spent on words that do work.

We should be honest about what kind of argument the last paragraph is. "Not needed" is the necessary half and it is not the whole of it: we also simply think the uncluttered line reads better, and that is a judgement about taste, made openly, not a deduction. A reader who finds `name: Type` more legible than `name Type` is not making an error we can point at; they are disagreeing about something real.

And the density has a cost that lands squarely on tooling and on eyes. A declaration with no keyword and no separator is the shortest it can be, which also makes it the least redundant: there is no second signal to catch a misread, so a name miscased is a line that means something else with nothing to flag it. Nor can you find declarations by searching for a word, the way `let ` or `var ` lets you in other languages — a Zane declaration has no distinguishing token to grep for, and finding them all is a job for something that knows the grammar rather than something that knows strings. That is a real loss of a cheap habit, and it is the direct price of the line being as short as it is.

## Every line admits to being a comment

Comments are the one place where the width argument runs the other way, and the language takes the wider form anyway. Zane has `//` and the `///` documentation form, and no block comment at all — a multi-line remark is a run of single-line ones, with a marker repeated on every line.

The case for `/* */` is that repeating the marker is waste: the remark is one thing, so one pair of delimiters should bracket it. That reading treats the repetition as a tax, and the whole decision turns on it being a feature instead. A block comment can run for hundreds of lines, and a reader does not always arrive at the top of one. They scroll into the middle of it, jump there from a search, or meet it as a hunk in a diff — and what they see is ordinary-looking text with nothing on the line to say it is inert, because the token that made it inert is somewhere above, possibly off the screen. Every `//` line answers that question about itself, from anywhere, at no distance. It is the same property the language reaches for everywhere else: a fact you can confirm by looking at the thing rather than at its context, the local-and-structural shape the [foundations story](foundations.md#the-verifiability-symmetry-and-the-one-gap-in-it) argues a reader can actually check.

Where this bites hardest is commented-out code, which is where large comment regions actually come from. A screen of code inside a `/* */` reads as code — it is indented like code, it is coloured like code by anything that has lost track of the opener, and only the delimiter far above says otherwise. The same screen with `//` on every line reads as switched off at every line, with no chance of mistaking one for live.

There is a smaller, purely practical reason alongside it. Line comments leave formatting alone. A block comment's continuation lines have to be aligned by hand against the opening token — three spaces, or whatever the opener's width leaves — and nothing re-aligns them when the block moves or the code around it re-indents. A `//` line is just a line; it indents with everything else.

The cost is paid by whoever is writing rather than reading. Commenting out thirty lines means thirty markers, and un-commenting them means removing thirty, where a block comment is two edits total. We are leaning on the editor for that — every editor has a toggle-comment command — and leaning on tooling for a comfort the language declines to provide is a real bet, not a free one. It is the usual trade in this language's favour: a comment is read many more times than it is written, and the read side is where we spend.

## The unnester that split what it unnested

The last form to leave was one that had been in the language almost from the start. Pipe syntax — `callableExpr|expr` — existed for unnesting. Deeply nested calls are genuinely hard to read, the innermost argument sitting furthest from the name that consumes it, and a pipe let the call name come first with its argument written after it instead of wrapped inside it.

How it survived as long as it did is the less flattering half. The language shed delimiters steadily as other rules claimed them — the bracket rule took `[ ]`, `( )`, and `{ }` for separated things, `$` went to package qualification, `&` to guests, `#` to identity, `'` to the loose operators, `@` to the reserved namespaces — and by the time anyone looked again, `|` was the last character not spoken for. That is not an argument for a feature. It is the observation that nothing else needed the token, and it kept the form alive through several rounds in which it would have lost a real contest.

Because the standing objection never went away: a pipe does not group. Parentheses hold a call and its argument together as one visual object, and nesting draws the nesting. `|` takes that object and cuts it in two, laying the halves side by side, so the reader reassembles the call from pieces instead of seeing it whole. That is not flattening; it is splitting, and a split reads *flatter* without reading *better*. Living with it for years never made it click.

What removed it was not that argument — that argument had been available the whole time — but the arrival of something that did the job properly. Once a call's last argument could [trail outside the parentheses](https://github.com/zane-lang/spec/blob/94d3833d094932802dfd96419db1b067e1271e03/spec/syntax.md#48-block-arguments-and-trailing-arguments) as a `{ }`, with no new token at all and the brace [ending the statement it closes](lexical.md#what-had-to-be-true-before-a-brace-could-end-a-statement), the unnesting pressure was answered where it actually came from. The calls that hurt to read nested were the ones with a *large* body at the call site, and those are exactly what the trailing brace takes. Pipe had been aiming at that case all along and hitting it with a general mechanism.

So removing it cost close to nothing, which is worth saying plainly rather than inventing a loss. Small arguments go back inside the parentheses, where they group. The only thing genuinely gone is unnesting for an argument that is not a `{ }` — and a call whose argument is one ordinary expression was never the call that was hard to read. What the language gets back is a shorter grouping rule: pipe occupied a level in the precedence table while explicitly not being an operator, an oddity the [operators story](operators.md#a-tier-below-everything) noted in passing when it listed the bounds on the loose forms. That observation stands as it was written; the level it referred to is now gone, and the loose tiers sit directly beneath the comparison operators with nothing between.

## A reference document with a history

One decision left, and it is this file's own existence. The rule until now was that the two reference documents get no story: `glossary.md` and `syntax.md` state, respectively, what the spec calls things and what the forms look like, and the reasoning behind any given rule was held to live with its *semantics*, in the topic document that owns it and the story beside that. On that reading `syntax.md` owns no decisions at all — it is a transcription of forms whose arguments happen elsewhere.

The chapters above are the counterexample. Which side of a declaration the type sits on is not a fact about what declaration *means*; no topic document owns it, and none of them would be the right place to argue it. The same is true of the width budget and the missing `:`, of the two orders and the swap between them, of the brackets that keep a function type from reading as a lambda, of the pipe and of the comment form. Every one of those is a decision about the **surface itself**, and before this file there was nowhere in the repository where any of them was written down as a decision rather than as a rule.

`glossary.md` keeps the exemption, and the asymmetry is not an oversight. A glossary entry's *why* is a naming argument, and naming arguments already have a home: the chapter that introduces a concept is required to defend the name it gives it, so a glossary story would be a story about terms that other stories are already responsible for. Nothing plays that role for form. A `> **Story:**` pointer from a syntax section had nowhere to point.

The line we now draw is that a form whose reasoning is about *what the construct means* belongs to the topic's story — a method call's `:` and `!` are argued in the [functions story](functions.md#mutation-you-can-see-at-the-call-site), because the argument there is about mutation and not about punctuation — while a form whose reasoning is about the *surface* — order, width, what can be told apart from what — belongs here. That line is a judgement call at the margins and we will get it wrong in both directions sometimes, putting an argument here that wanted its topic's story or leaving one there that nobody looking at the grammar would ever find. It is still better than the arrangement it replaces, in which the second kind of reasoning had no home at all and simply went unrecorded.

## The word every namespace shares

The line just drawn gets an early test in the `@` sigil, where the decision is about a name on the surface and nothing the named things do. `@` marks what the compiler supplies rather than a package declares, and for most of its life it opened two namespaces, both named for what their members were: `@primitives$` for storage and `@concepts$` for the types that only ever stand in a parameter. When branching, repetition, and exiting left the grammar and became calls ([the control-flow story](control-flow.md#two-intrinsics-and-what-they-are-stated-over)), three operations needed a home behind the same sigil, and the name that came first to hand was `@intrinsics$` — the word every compiler toolchain already uses for an operation it builds in.

It did not survive the first hard look, because the word was already describing the whole sigil. A storage primitive is exactly as much the compiler's own as `branch` is; nothing about `@primitives$Int` is less intrinsic than an operation over it. So `@intrinsics$` could only mean one of two things, and both were wrong. Either it claimed the word for three operations, and the rest of the `@` space was left with no name to be spoken of together; or the word meant everything behind `@`, and a namespace carrying it distinguished nothing, since the `@` had already said as much before the name was read.

We looked at keeping the namespace and coining a fresh umbrella term for the whole space instead. That fought the language rather than the name: the everyday sense of "intrinsic" — built into the thing itself — is the whole-space meaning already, and a coined rival would have spent its life being explained as "what you would have called intrinsic". So the word went to the whole space and the namespace gave it up ([`syntax.md` §2.7](https://github.com/zane-lang/spec/blob/c6a2d7acca15b8e250aa2d2035dc270ea654e8a7/spec/syntax.md#27-intrinsic-namespaces)): an **intrinsic** is anything reached through `@`, the `@` namespaces are the intrinsic namespaces, and the operations moved to `@controlflow$`, named for what they are for. The rule that falls out is the one worth keeping for whatever joins the sigil next: a namespace is named for what its members are or do, and never for the one thing all of them are.

The definition had a casualty on its first day. `Array<T, n>` had always been described as the storage primitive the compiler provides, and it was written bare — the one compiler-supplied name in the language that no `@` announced. Once "intrinsic" meant everything behind the sigil, that left `Array` either an intrinsic outside the intrinsic namespaces or an exception the definition had to carry, and neither was worth keeping for the sake of a shorter spelling. So the primitive moved to `@primitives$Array<T, n>` and `core` wraps it as `Array` ([`generics.md` §8](https://github.com/zane-lang/spec/blob/5e11c13450eba31f9560a7d04ba576fe6448191b/spec/generics.md#8-container-storage-primitives)), on exactly the terms it already wrapped `@primitives$Int` as `Int`. Source did not change: examples still write `Array<Int, 10000>`, and the constructors that read a length off an array literal are `core`'s, as the constructors that read an `Int` off a number literal always were. What changed is that `Array` became a fundamental type in the plain sense the spec gives that phrase — a type `core` declares — and that it gained a sibling in the same namespace, `@primitives$List<T>`, which `core` wraps as `List`.

The cost is that the bare word no longer means an operation. That is the sense a reader arriving from C or Rust brings, where an intrinsic is a function the compiler implements specially, so prose that means the three operations has to say "control-flow intrinsic" in full, and a reader who skims past the qualifier can take a claim about the operations as a claim about every type behind `@`. Stretching the word over types is not new — Fortran has called its built-in types intrinsic types for decades — but it is a stretch against the more common habit, and the spec pays for it with the extra word wherever the narrower meaning is meant.
