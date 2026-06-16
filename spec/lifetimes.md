# Zane Lifetimes

This document specifies Zane's lexical lifetime rules: `&` assignment scope checks, ownership moves, and deterministic destruction. It builds on the ownership and storage forms defined in [`memory.md`](memory.md).

> **See also:** [`memory.md`](memory.md) §2 for ownership and storage, §4 for anchors and refs. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`effects.md`](effects.md) §2 for `mut`.

---

## 1. Scope Rules and Moves

### 1.1 `&` assignment uses owner scope
An `&` assignment is legal only when the target's owner is declared in the same or a higher lexical scope than the `&` itself.

```zane
outer Node()
r &Node = outer
{
    innerNode Node()
    r = innerNode // ILLEGAL: owner's scope is nested relative to the ref
}
```

The compiler compares declaration scopes. It does not perform borrow inference or lifetime annotation solving.

### 1.2 Move-sources are owning symbols or owned verb results
A move-source must denote an **owned value the expression is entitled to consume**. Two forms qualify:

- a **direct owning symbol**: an owning local binding or owning parameter named directly by an identifier expression
- an **owned verb result**: a value returned by a verb (function, method, operator, constructor, or lambda) whose return type is an owned `T`

A verb that returns an owned `T` produces a fresh value that no symbol, field, or container owns yet. Moving it transfers ownership of that temporary straight into the destination, so it re-parents nothing.

The following are **not** move-sources:
- an `&` value, including a verb that returns `&T` (refs are non-owning and cannot transfer ownership; see [`memory.md`](memory.md) §2.4)
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

### 1.3 Moves are restricted to the declaration block
A direct owning symbol may only be used as a move-source in the exact lexical block where that symbol was declared. Owning parameters are treated as declared at the top of the function body block.

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
    this.cars!append(car) // legal: parameter car is treated as declared at function body block top
}
```

This restriction prevents conditional moves and flow-dependent ownership changes. If control flow is needed, compute the destination or guard condition first, then perform a single move in the symbol's declaration block.

The restriction applies only to symbol move-sources. An owned verb result (§1.2) is an unnamed temporary with no declaration block, so it is simply consumed at the point where it appears.

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

### 1.5 Callee-decides move semantics
When a value is passed to a function, the callee decides whether a move happens by what it does internally:

- storing the value in an owning slot moves it
- not storing it leaves ownership in place

For `&` fields specifically, the callee must declare the corresponding parameter as `&T` ([`memory.md`](memory.md) §2.9). Attempting to bind a plain `T` parameter into `&` storage is a compile-time error. This means the callee's signature signals whether an `&`-creating source ([`memory.md`](memory.md) §2.8) is required at the call site.

### 1.6 Moved symbols downgrade to `&` values and are no longer movable
After a direct owning symbol is moved, that symbol is downgraded to an `&` value through the anchor (see [`memory.md`](memory.md) §4.5). The symbol remains readable but cannot be moved again.

```zane
engine Engine()
car Car(engine)          // engine is moved; downgrades to `&`
engine:inspect()         // legal: engine is now an `&`, still readable
truck Truck(engine)      // ILLEGAL: engine is an `&`, not a move-source
```

This also applies across calls: if a callee moves a caller-owned value, the caller can still read the symbol afterward through the downgraded `&`. Zane therefore has no user-visible use-after-move error class for reads.

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

---

## 2. Lifetime and Destruction

### 2.1 Destruction is deterministic
Class instances are destroyed when their owner dies, their owning container dies, or their owning scope drains under the concurrency rules.

### 2.2 Scopes drain before destruction
If a scope launches concurrent work, objects owned by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule (see [`concurrency.md`](concurrency.md) §4.1).

### 2.3 Ref storage never extends lifetime
Refs do not participate in ownership and cannot prolong object lifetime. They only track a live object whose owner is already guaranteed to outlive them.

### 2.4 Null refs are not a user-facing state
Because scope rules (§1.1) prevent refs from outliving their owners, the runtime does not expose a normal “null ref” programming model to the user.

---

## 3. Language Comparisons

### 3.1 Lifetime and destruction behavior

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Destruction timing | deterministic | non-deterministic | deterministic | manual / RAII |
| GC pauses | ❌ | ✅ | ❌ | ❌ |
| Dangling ref risk | ❌ | ❌ | ❌ | ✅ |
| Lifetime annotations | ❌ | ❌ | ✅ | ❌ |

---

## 4. Design Rationale

| Decision | Rationale |
|---|---|
| Move-sources are owning symbols or owned verb results | A symbol and a fresh verb result are each owned by no other object, so consuming either cannot re-parent an existing ownership subtree. Fields and container elements stay non-movable, keeping containers stable and ownership predictable. |
| Refs are never move-sources | Refs are non-owning aliases. Allowing moves from refs would enable re-parenting through alias paths and break the owner-uniqueness property. |
| Moves restricted to declaration block | Prevents conditional moves and flow-dependent ownership changes. A symbol's ownership transfer happens at most once, in a predictable location. This eliminates the need for flow-sensitive "maybe moved" analysis. |
| Moved symbols downgrade to refs | Moved-from symbols remain usable for reads through anchor-based ref downgrade. This preserves ergonomics while preventing double-moves and re-parenting. |
| Lexical scope rules | A simple same-or-higher-scope rule is easy to implement and explain. |
| Callee-decides moves with caller downgrade | Preserves ownership while avoiding Rust-style use-after-move friction. |
| Water-tower destruction | Extends the same ownership model into concurrent execution without adding GC or async lifetimes. |

---

## 5. Summary

| Concept | Rule |
|---|---|
| `&` return | Returned `&T` must be rooted in a parameter; `this` counts |
| Ref assignment | Only from a place expression whose owner is in the same or a higher lexical scope than the ref |
| Move-source | A direct owning symbol (local or parameter) or an owned verb result; not an `&`, field, container element, or other access path |
| Move declaration-block restriction | A direct owning symbol may only be moved in the exact lexical block where it was declared; parameters are treated as declared at function body top |
| Move destination scope | Destination owner must be in the same or a higher lexical scope than the source owner |
| Post-move downgrade | After a move, the source symbol downgrades to an `&` and remains readable but is no longer a move-source |
| Function call | Callee decides move; caller symbol remains usable |
| Destruction | Deterministic and delayed until the owning scope drains |
