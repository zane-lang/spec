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

and the short form is [shorthand for the long one](https://github.com/zane-lang/spec/blob/a3760ba3179eb827acb0eefd7b885504e7db29e6/spec/syntax.md#11-symbols) — `number Int(2)` means `number Int = Int(2)`, which is the field write with a type annotation in front of the destination. The two lines are now the same line with and without a declaration on it. `Int` appears once as a type and once as a constructor, in that order, in both; the shorthand's whole content is that when those two are the same name you may write it once.

The cost is a token, and it is fair to name it rather than pretend the shorthand is free. Written out, `number Int = Int(2)` says `Int` twice, and the reason the language offers a shorthand at all is that the doubling is real and would otherwise be paid on every line. We are buying explicitness with repetition, collapsing the repetition where we can, and still paying it at every field write — `player.health = Int(20)` names the type each time, forever, where a language with inference writes `player.health = 20`. That is the bill for refusing inference and we do not think it can be argued away; the position is that the token is worth what it says.

The second cost is subtler and it is a debt owed to a different document. `number Int(2)` is two identifiers with nothing between them, and it is only readable because the second one is obviously a type — which it is only because [casing carries the kind](lexical.md#casing-carries-the-kind). Written in a language where `number int(2)` were legal, this form would be unreadable mush. The order we chose is affordable because of a rule that was decided on other grounds, and it would not survive that rule being relaxed.

## One shape, and everywhere it turned up

Having settled the order for one kind of declaration, we did not go on to design forms for the others. The shape `name Type(args)` turned out to fit them already, which is the sort of thing that reads as design and mostly was not; the forms met because each was the same act underneath — name a place, say what it holds, build what goes in it.

A [lambda variable](https://github.com/zane-lang/spec/blob/a3760ba3179eb827acb0eefd7b885504e7db29e6/spec/syntax.md#38-lambda-literals-and-lambda-variable-declarations) is the clearest case. The [functions story](functions.md#pulling-methods-out-of-the-type-body) records how a lambda literal turned out to be an ordinary verb declaration with the name deleted; put a name back in front of one and you have written the declaration shorthand again, with a function type where `Int` would be and a body where the constructor arguments would be:

```zane
callback Unit(this Player) mut {
    this.shooting = Bool(false);
    return Unit();
}
```

which expands the same way anything else does — to a symbol of function type bound to the literal. Nobody designed a lambda-variable syntax. The shorthand covered it.

[Named constructors](https://github.com/zane-lang/spec/blob/a3760ba3179eb827acb0eefd7b885504e7db29e6/spec/types.md#34-named-constructors) and variant cases slot in the same way: the type position accepts a qualified `Type.member`, so `v Vector2.diagonal(Float(3))` and `e Expr.intLit("5")` are the one form with a longer name in the type slot, and the [types story](types.md#named-constructors-and-the-syntax-variants-already-had) tells how those two arrived at one spelling from opposite directions. A package constant is the same line at package scope. And a defaulted entry in a [field-constructor header](https://github.com/zane-lang/spec/blob/a3760ba3179eb827acb0eefd7b885504e7db29e6/spec/syntax.md#34-field-constructors) is written `name String("Pistol")` — a constructor parameter with a default, spelled exactly like the declaration it will initialize, because it *is* that declaration form reused.

There is one seam in all this reuse and it is worth pointing at, because it is the kind of thing a reader trips over once and remembers with irritation. A bare `name Type`, with no construction after it, is not a declaration — symbols require [direct initialization](https://github.com/zane-lang/spec/blob/4026aba01b3e07b28cf0476b79e32a354f8c22c3/spec/memory.md#211-symbols-require-direct-initialization) and the bare form is illegal as a statement. But inside a field-constructor header the very same text is legal and means something else: a required input the caller must supply. So the same two tokens are an error in one brace and a parameter in another, and the only thing distinguishing them is which body they sit in. We accept it because a required constructor input genuinely has no value to write — that is what "required" means — but it is a real wrinkle, and the alternative of minting a separate marker for it would have spent grammar to make a rule that context already settles.

## Words are paid for by the line

The other standing pressure on Zane's surface is width. This is not the familiar claim that keywords are bad; keywords are fine, and the language keeps the ones that earn it. The claim is narrower and more mechanical: a signature is where a language spends its horizontal budget, and a Zane signature is already carrying a return type, an abort type, a subject, parameter names with their types, and possibly a `mut`. Every word added to that line is width spent on grammar rather than on what the code is about, and past some length a signature stops being read and starts being skimmed. So words are bought, not assumed.

That is why there is no `var` or `let` in front of a declaration, and no `func` or `def` in front of a function. Neither would tell a reader anything the line does not already say — a name followed by a type followed by arguments is a declaration whatever word precedes it, and a return type followed by a name followed by a parameter list is a function. A word whose whole content is to announce the construction it precedes is a word the construction announces itself.

The same reasoning retires the `:` that Pascal, ML, TypeScript, Rust, and most of their descendants put between a name and its type. Its job is to signal that what follows is a type, and it is a perfectly good signal in a language where `name type` and `name value` are indistinguishable strings. In Zane they are not: the casing rule means a type's first letter has already given the answer by the time the `:` would arrive. The separator carries no information anything else does not already carry, so it goes. What Zane does keep is every word that names an act nothing else names — `type`, `alias`, `import`, `return`, `match`, `spawn`, `implicit`, `mut`, `this` — and it went as far as removing `if` and `while` from the grammar entirely, which the [control-flow story](control-flow.md#doing-without-while) tells as its own episode. The budget is not zero. It is spent on words that do work.

We should be honest about what kind of argument the last paragraph is. "Not needed" is the necessary half and it is not the whole of it: we also simply think the uncluttered line reads better, and that is a judgement about taste, made openly, not a deduction. A reader who finds `name: Type` more legible than `name Type` is not making an error we can point at; they are disagreeing about something real.

And the density has a cost that lands squarely on tooling and on eyes. A declaration with no keyword and no separator is the shortest it can be, which also makes it the least redundant: there is no second signal to catch a misread, so a name miscased is a line that means something else with nothing to flag it. Nor can you find declarations by searching for a word, the way `let ` or `var ` lets you in other languages — a Zane declaration has no distinguishing token to grep for, and finding them all is a job for something that knows the grammar rather than something that knows strings. That is a real loss of a cheap habit, and it is the direct price of the line being as short as it is.
