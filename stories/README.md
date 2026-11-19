# Design stories

The spec states *what* the language does. The stories, one per spec document, tell *why*: the situation that forced each choice, the roads not taken, and the costs accepted, in the order the thinking moved. They are appended to as the design changes, so each is the full record of how its part of the language came to be.

| Story | Behind |
|---|---|
| [`foundations.md`](foundations.md) | [`spec/foundations.md`](../spec/foundations.md): the bets behind captured intent, staged compilation and strictness as performance |
| [`syntax.md`](syntax.md) | [`spec/syntax.md`](../spec/syntax.md): why a declaration writes its name before its type, and how one shape came to cover constructors, constants and function values |
| [`lexical.md`](lexical.md) | [`spec/lexical.md`](../spec/lexical.md): casing as a hard signal for what a name is, and letting the bracket choose the separator |
| [`types.md`](types.md) | [`spec/types.md`](../spec/types.md): the value/reference model, why `#` marks a distinct type, and how constructors and literals reach storage |
| [`adt.md`](adt.md) | [`spec/adt.md`](../spec/adt.md): splitting `enum` from `variant`, the central `match` block, and how a variant can contain itself |
| [`generics.md`](generics.md) | [`spec/generics.md`](../spec/generics.md): one parameter model for types and sizes, the `<>`/`()` split, and why a second container primitive was needed |
| [`dependencies.md`](dependencies.md) | [`spec/dependencies.md`](../spec/dependencies.md): packages identified by URL, the manifest/lock split, prebuilt releases, and `core` as an ordinary package |
| [`memory.md`](memory.md) | [`spec/memory.md`](../spec/memory.md): memory safety with no garbage collector and no lifetime annotations: owners, references and arenas |
| [`lifetimes.md`](lifetimes.md) | [`spec/lifetimes.md`](../spec/lifetimes.md): lexical scope in place of a borrow checker, and the single owner comparison made at every store |
| [`effects.md`](effects.md) | [`spec/effects.md`](../spec/effects.md): inferring effects instead of annotating them, and capabilities in place of ambient I/O |
| [`concurrency.md`](concurrency.md) | [`spec/concurrency.md`](../spec/concurrency.md): implicit parallelism without `async` coloring, and why `spawn` marks only a call |
| [`error-handling.md`](error-handling.md) | [`spec/error-handling.md`](../spec/error-handling.md): failure as a second return path rather than a `Result` value |
| [`control-flow.md`](control-flow.md) | [`spec/control-flow.md`](../spec/control-flow.md): how branching and repetition left the language and became ordinary `core` code |
| [`functions.md`](functions.md) | [`spec/functions.md`](../spec/functions.md): methods pulled out of the type body, mutation made visible with `:` and `!`, and overloading by shape |
| [`operators.md`](operators.md) | [`spec/operators.md`](../spec/operators.md): a fixed set of overloadable operators, laws enforced through derived operators, and no implicit coercion |
| [`math.md`](math.md) | [`spec/math.md`](../spec/math.md): math as compiler intrinsics with a `math` package over them |
| [`packages.md`](packages.md) | [`spec/packages.md`](../spec/packages.md): the directory as the package, explicit `$` access, and the `lib/`, `bin/` and `test/` layout |
| [`ffi.md`](ffi.md) | [`spec/ffi.md`](../spec/ffi.md): calling C, whose calls can fold at compile time, and which never writes Zane memory |
