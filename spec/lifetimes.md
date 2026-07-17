# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: `&` assignment scope checks, ownership moves, and deterministic destruction. It builds on the ownership and storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for ownership and storage, §4 for anchors and tethers. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 `&` assignment uses owner scope
An `&` assignment is legal only when the target's owner is declared in the same or a higher lexical scope than the `&` itself.

```zane
outer Node()
r &Node = outer
{
    innerNode Node()
    r = innerNode // ILLEGAL: owner's scope is nested relative to the tether
}
```

The compiler compares declaration scopes. It does not perform borrow inference or lifetime annotation solving.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#inheriting-a-debt-safety-without-a-borrow-checker) — "Inheriting a debt: safety without a borrow checker".

### 1.2 Move-sources are owning symbols or owned verb results
A move-source must denote an **owned value the expression is entitled to consume**. Two forms qualify:

- a **direct owning symbol**: an owning local binding or owning parameter named directly by an identifier expression
- an **owned verb result**: a value returned by a verb (function, method, operator, constructor, or lambda) whose return type is an owned `T`

A verb that returns an owned `T` produces a fresh value that no symbol, field, or container owns yet. Moving it transfers ownership of that temporary straight into the destination, so it re-parents nothing.

The following are **not** move-sources:
- an `&` value, including a verb that returns `&T` (tethers are non-owning and cannot transfer ownership; see [`memory.md`](memory.md) §2.4)
- a field access such as `car.engine`
- a container element access such as `cars[1]`
- any other access path that projects into an existing owner

```zane
engine Engine()
car Car(engine)             // legal: engine is a direct owning symbol
boat Boat(makeEngine())     // legal: makeEngine() returns an owned Engine

truck Truck(car.engine)     // ILLEGAL: field access is not a move-source
truck2 Truck(makeCar().engine) // ILLEGAL: field access on temporary is not a move-source
garage Garage(cars[1])      // ILLEGAL: container element is not a move-source
```

This rule keeps containers stable ownership subtrees. Once a value is owned by a field or stored in a container element, it cannot be individually moved out. The containing object may be moved as a whole if it is itself a move-source. An owned verb result is exempt from the access-path restriction because it is owned by no one until the move binds it.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#what-may-be-moved-keeping-ownership-subtrees-whole) — "What may be moved: keeping ownership subtrees whole".

### 1.3 Moves are restricted to the declaration block
A direct owning symbol may only be used as a move-source in the exact lexical block where that symbol was declared. Owning parameters may be used as move-sources at the top level of the function body. A parameter is not part of the body scope, though: it belongs to the **call-site scope** (§1.5). The caller that supplied an owning argument has already downgraded to a tether (§1.8); moving the parameter within the body only decides where the value comes to rest.

```zane
engine Engine()
car Car(engine)          // legal: same block as engine's declaration

{
    node Node()
    innerOwner Node = node // legal: same block as node's declaration
}
```

Moving an outer symbol from a nested block is illegal:

```zane
car Car()
{
    garage Garage(car)   // ILLEGAL: car was declared in outer block
}
```

```zane
Void loadCar(this Boat, car Car) mut {
    this.cars!append(car) // legal: car is moved into this.cars at the top level of the body
}
```

This restriction prevents conditional moves and flow-dependent ownership changes. If control flow is needed, compute the destination or guard condition first, then perform a single move in the symbol's declaration block.

The restriction applies only to symbol move-sources. An owned verb result (§1.2) is an unnamed temporary with no declaration block, so it is simply consumed at the point where it appears.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-declaration-block-rule-and-the-flow-analysis-it-refuses) — "The declaration-block rule, and the flow analysis it refuses".

### 1.4 Destination scope must contain or match source scope
A value may move into a new owner only when the destination owner is declared in the same or a higher lexical scope than the source owner.

```zane
node Node()
{
    nestedOwner Node()
    nestedOwner = node // ILLEGAL: cannot move into an owner declared in a nested scope
}
```

An owned verb result (§1.2) has no source owner; its source scope is the expression that produces it. That scope is always nested within or equal to the destination owner's scope, so this restriction is trivially satisfied and never blocks moving a verb result into any owner.

A parameter's value is exempt. Because a parameter belongs to the call-site scope and is not part of the body (§1.5), lending it into a local or a nested call does not sink ownership into that lower scope: the value returns to the call site when the local exits, unless the callee moves it into another parameter's owning storage or into the return (§1.8).

### 1.5 Parameters belong to the call site
A reference-type parameter is **not part of the callee's body scope**. It behaves as a symbol in the **call-site scope**, one level above the body. Passing an owned reference value to a plain `T` parameter lends it in with owning access, but the value's lifetime stays with the call site.

This is what makes the passing rule safe. Because the parameter is not part of the body scope, the body draining never destroys the value. The body may read it, move it into a local, or pass it to a nested call; when a local that received it exits, the value is not dropped — the compiler moves it back up to the call site, and the chain repeats outward until the scope that first owned the value drains. A value passed by owning access therefore always outlives the call, which is what lets the caller's symbol downgrade to a live tether (§1.8) rather than a dangling one.

```zane
Void enterMatch(player Player) {
    island Island = makeIsland()
    island!startMatch(player) // player is lent into the local island
}
```

`startMatch` puts `player` into the local `island`. Because `player` belongs to the call site, `island` draining does not destroy it; the value lives until `enterMatch`'s own scope drains. Inside `enterMatch`, `player` was passed to `startMatch` by owning access, so `enterMatch`'s `player` symbol is now a tether to it (§1.8) — and so is the argument symbol in whatever called `enterMatch`.

For `&` fields specifically, the callee must declare the corresponding parameter as `&T` ([`memory.md`](memory.md) §2.9). Attempting to bind a plain `T` parameter into `&` storage is a compile-time error, because a swallowed value is owned at the call site while an `&` field lives with the object that holds it, which may outlive the call. The callee's signature therefore signals whether an `&`-creating source ([`memory.md`](memory.md) §2.8) is required at the call site.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#consumed-or-borrowed-the-parameter-that-lives-at-the-call-site) — "Consumed or borrowed: the parameter that lives at the call site".

### 1.6 Moved symbols downgrade to `&` values and are no longer movable
After a direct owning symbol is moved, that symbol is downgraded to an `&` value through the anchor (see [`memory.md`](memory.md) §4.5). The symbol remains readable but cannot be moved again.

```zane
engine Engine()
car Car(engine)          // engine is moved; downgrades to `&`
engine:inspect()         // legal: engine is now an `&`, still readable
truck Truck(engine)      // ILLEGAL: engine is an `&`, not a move-source
```

This also applies across calls. Passing an owning value to a plain `T` parameter downgrades the caller's symbol to an `&` (§1.8); the caller can still read the symbol afterward through that downgraded `&`. Zane has no user-visible use-after-move error class for reads.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#downgrade-not-poison-why-there-is-no-use-after-move-read) — "Downgrade, not poison: why there is no use-after-move-read".

An owned verb result (§1.2) has no symbol to downgrade. The temporary is consumed by the move and cannot be named again, so the double-move question never arises for it.

### 1.7 Returned `&` values must be rooted in a parameter
A function may return an `&T` only when the returned reference is rooted in one of the function's parameters. `this` counts as a parameter for this rule.

```zane
&Weapon getWeapon(this &Player) => this.weapon
```

```zane
&Int bad() {
    value Int = 1
    return value   // ILLEGAL: returned `&` is not rooted in a parameter
}
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#returning-a-ref-without-a-lifetime-to-name-it) — "Returning a ref without a lifetime to name it".

### 1.8 Passing an owner to a `T` parameter downgrades it to a tether
A plain reference-type parameter `T` takes its argument by **owning access**. Passing an owning value to such a parameter uses that value as a move-source (§1.2), so the caller's symbol downgrades to a tether (§1.6) — **whatever the callee does with the value**. The parameter's declared type is the whole contract: `T` means the caller gives up ownership; `&T` (a tether, [`memory.md`](memory.md) §2.9) means the caller lends the value and stays a full owner. Nothing in the callee's body changes the outcome the signature already states.

```zane
car Car()
garage!store(car)     // store takes `Car`: car downgrades to a tether
car:inspect()         // legal: car is still readable through the tether
truck Truck(car)      // ILLEGAL: car is a tether, not a move-source
```

The value outlives the call (§1.5), so the downgraded tether always resolves to a live object. Where the value comes to rest — moved into another parameter's owning storage, moved into the return, or held in the call-site scope — the tether follows through the anchor ([`memory.md`](memory.md) §4.5).

To keep a full owner, do not hand it to a plain `T`:

- pass `&T` to lend the value for the duration of the call — the passing mode for reading a reference object ([`memory.md`](memory.md) §2.9); or
- have the verb **return** the owner, and bind it at the call site (§1.9).

```zane
weapon Weapon()
weapon2 Weapon = reforge(weapon)   // reforge returns the owner; weapon2 is a fresh owner
```

A verb that only reads its reference argument may still declare it plain `T`: reading does not change the fact that the signature asked for an owner, so the caller downgrades all the same. Declaring the parameter `&T` is what keeps the caller an owner. Because the signature alone decides the caller's state, there is no interprocedural consumption inference: whether a passed owner downgrades never depends on the callee's body or on the build. Using an owner only to read it is legal. Leaving a parameter entirely unused is a separate, general matter — a release build rejects an unused parameter whether it is an owner or not.

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#the-signature-is-the-whole-contract-retiring-inferred-consumption) — "The signature is the whole contract: retiring inferred consumption".

### 1.9 A non-`Void` return value cannot be ignored
Returning an owner is how a verb hands ownership back to the caller (§1.8), so discarding a return could silently drop an owned value — and strand any tether the caller still holds to it. Every non-`Void` return value must therefore be **bound or consumed** at the call site; it may not be discarded.

```zane
render(player)                       // legal: render returns Void
newPlayer Player = respawn(player)   // legal: non-Void return is bound
respawn(player)                      // ILLEGAL: a non-Void return may not be ignored
```

> **Story:** [`stories/lifetimes.md`](../stories/lifetimes.md#consumed-or-borrowed-the-parameter-that-lives-at-the-call-site) — "Consumed or borrowed: the parameter that lives at the call site".

---

## 2. Lifetime and Destruction

### 2.1 Destruction is deterministic
Class instances are destroyed when their owner dies, their owning container dies, or their owning scope drains under the concurrency rules.

### 2.2 Scopes drain before destruction
If a scope launches concurrent work, objects owned by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule (see [`concurrency.md`](concurrency.md) §4.1).

### 2.3 Tether storage never extends lifetime
Tethers do not participate in ownership and cannot prolong object lifetime. They only track a live object whose owner is already guaranteed to outlive them.

### 2.4 Null tethers are not a user-facing state
Because scope rules (§1.1) prevent tethers from outliving their owners, the runtime does not expose a normal “null tether” programming model to the user.

---

## 3. Language Comparisons

### 3.1 Lifetime and destruction behavior

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Destruction timing | deterministic | non-deterministic | deterministic | manual / RAII |
| GC pauses | ❌ | ✅ | ❌ | ❌ |
| Dangling tether risk | ❌ | ❌ | ❌ | ✅ |
| Lifetime annotations | ❌ | ❌ | ✅ | ❌ |

---

## 4. Summary

| Concept | Rule |
|---|---|
| `&` return | Returned `&T` must be rooted in a parameter; `this` counts |
| Tether assignment | Only from a place expression whose owner is in the same or a higher lexical scope than the tether |
| Move-source | A direct owning symbol (local or parameter) or an owned verb result; not an `&`, field, container element, or other access path |
| Move declaration-block restriction | A direct owning symbol may only be moved in the exact lexical block where it was declared; parameters may be moved at the body top level |
| Move destination scope | Destination owner must be in the same or a higher lexical scope than the source owner |
| Post-move downgrade | After a move, the source symbol downgrades to an `&` and remains readable but is no longer a move-source |
| Parameter scope | A reference parameter belongs to the call-site scope, not the body, so a value passed by owning access outlives the call |
| Owning argument | Passing an owning value to a plain `T` parameter downgrades the caller's symbol to a tether, whatever the body does; `&T` lends the value and keeps ownership, and a verb hands an owner back by returning it |
| Return value | A non-`Void` return must be bound or consumed at the call site; it cannot be ignored |
| Destruction | Deterministic and delayed until the owning scope drains |
