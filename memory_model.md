# Zane Memory Model

This document specifies Zane's memory model: ownership, refs, anchors, lexical lifetime rules, heap layout, and deterministic destruction.

> **See also:** [`oop.md`](oop.md) §2 for classes and structs. [`purity.md`](purity.md) §2 for `mut`. [`concurrency_model.md`](concurrency_model.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling references by combining single ownership, lexical lifetime rules, and anchor-based tracking of non-owning references.

- **`Overwritable owners`.** A class owner is directly initialized and may later be overwritten.
- **`Repointable refs`.** An `&` value is non-owning storage that can point at different owners over time.
- **`Lexical lifetime enforcement`.** Ref assignment and ownership moves are checked using declaration scope alone.
- **`Deterministic destruction`.** Objects are destroyed when their owning scope drains; there is no tracing garbage collector.

These rules fit together mechanically. Owners are the only storage that controls destruction. Refs may point only at existing places, never temporaries. Lexical scope checks ensure the owner outlives every ref derived from it. When ownership moves or an owner is overwritten, refs stay valid because they follow the owner/anchor path rather than a fixed object address.

---

## 2. Ownership and Storage

### 2.1 Every class instance has exactly one owner
Every class instance is owned by exactly one symbol, field, or container slot at a time. Ownership is the default storage mode for class values.

### 2.2 Class owners are overwritable after initialization
Any owning storage position for a class instance—a symbol, field, or container slot—**MUST** be directly initialized, and **MAY** later be overwritten.

```zane
tank Tank(...)
tank = Tank(...) // legal
```

Overwriting an owner does not invalidate existing refs. Refs follow the owner/anchor path, so later reads observe the owner's current value.

Container overwrite therefore does not depend on whether the element slot stores an owner or an `&` value. Both kinds of slots may be rewritten after initialization.

```zane
owners Array2 = [Node(), Node()]
```

Rewriting `owners[1]` replaces the owned class instance in that slot. Refs to that slot observe the new value because refs follow the owner/anchor path, not the original object.

### 2.3 Struct values are freely overwritable
Structs are value types with no anchor and no heap identity. Reassigning a struct overwrites the storage slot directly.

```zane
pos Vec2(1, 2)
pos = Vec2(3, 4) // ok
```

### 2.4 `&` is non-owning storage
`&` creates non-owning storage. An `&` value may be declared as:

- a local symbol
- a class field
- an element type inside another storage type
- a function or constructor parameter
- a function return type

An `&` type is legal in storage sites (local symbols, fields, nested storage types), function parameter positions, and function return-type positions.

### 2.5 Refs are repointable
An `&` symbol or `&` field may be assigned a different target later, as long as the scope rule in §3.1 is satisfied.

### 2.6 Refs are independent `&` values
Assigning or passing an `&` value gives the destination its own `&` value to the same owner. Rebinding one `&` storage site later changes only that storage site; it does not retarget other `&` values that already point to that owner.

### 2.7 Refs and owners use the same surface operations
At use sites, an `&` value is used with the same surface syntax as a direct owner. Method calls, field access, and `mut` calls use the ordinary syntax. The distinction between owner and `&` matters only at the storage site: an `&` value stores a non-owning link, while an owner stores the object itself or its owning slot.

### 2.8 Place expressions and new `&` values
A **place expression** is an expression that denotes an existing, stable storage location.

The following are place expressions:

- a named local, field-backed, or owning/`&` storage symbol such as `engine`
- a field access whose base is a place, such as `car.engine` or `this.engine`
- a subscript expression `list[index]` when `list` is a place expression and `[]` is defined as a place projection for that receiver type
- an `&T` parameter inside the callee body (§2.9)

Only some place expressions may create a new `&` value. A new `&` binding may be initialized from:

- a named symbol
- a field access whose base is a place
- an `&T` parameter

A `[]` expression is never a source for creating a new `&`, even when it is a place expression.

Temporaries and other value-only expressions are not place expressions. Constructor calls and ordinary function results such as `Engine()` and `makeEngine()` are not places and cannot be bound to an `&`.

```zane
engine &Engine = Engine()   // ILLEGAL: Engine() is a temporary, not a place expression
```

```zane
engine Engine()
r &Engine = engine   // legal: engine is a named, stable storage location
```

```zane
weapons List = [Weapon(), Weapon()]
current &Weapon = weapons[1]   // ILLEGAL: `[]` cannot create a new `&`
```

```zane
first Weapon()
second Weapon()
weapons List = [first, second]
current &Weapon = weapons[1]   // legal: uses the existing stored `&Weapon`
```

This works because `weapons[1]` reads an `&Weapon` value that is already stored in the list. It does not create a new `&` from an owning element. Those stored `&` values are stable because the language does not let `[]` create `&` values from owner storage in the first place.

Non-`&` owner bindings may be initialized from any expression, including temporaries. The owner materializes the value into stable storage.

```zane
engine Engine()         // legal: plain owner binding; Engine() temporary is materialized into engine
```

### 2.9 `&` function parameters
A parameter declared as `&T` requires the caller to supply a source that may create a new `&` under §2.8. Inside the callee body it acts as a place expression and may be stored into `&` storage or returned as `&T` under §3.7.

A parameter declared as plain `T` is a **value-only binding**. The caller is not required to supply a place expression. A plain `T` parameter does not guarantee a stable `&`-rootable source location, therefore it **MUST NOT** be bound into `&` storage or returned as a new `&T`. Inside the callee body, a plain `T` parameter is not a place expression for `&`-binding purposes.

```zane
class Car {
    engine &Engine
    _value Int
}

// legal: `&` parameter allows storing into `&` field
Void consume(this Car, engine &Engine) mut {
    this.engine = engine   // legal
}

// different function with a plain parameter
Void inspect(this Car, engine Engine) {
    return this._value + engine.speed   // legal: reading only
}
```

Binding a plain parameter into `&` storage is illegal:

```zane
Void consumeWrong(this Car, engine Engine) mut {
    this.engine = engine   // ILLEGAL: plain parameter is not `&`-rootable
}
```

This rule preserves uniform call syntax. The call site writes `consume(e)` or `inspect(e)` regardless of whether the parameter is `&`. The callee's signature determines whether an `&`-creating source is required from the caller.

### 2.10 Struct-downstream enforcement (transitive struct field restrictions)
Structs form a closed world of plain value storage. A struct field may contain primitives (see [`syntax.md`](syntax.md) §2.1) and other structs, but it **MUST NOT** contain a class or an `&`. This rule applies transitively: a struct containing another struct that eventually contains a class or `&` is also illegal.

Here, **downstream** means "through nested struct fields." The restriction is checked recursively through the full struct graph.

Structs are copied and overwritten as ordinary inline values. They do not have per-instance anchors or destruction tracking. If a struct could contain a class field, copying the struct would silently duplicate ownership. If a struct could contain an `&`, copying the struct would silently duplicate non-owning tracking state without going through the anchor system. Downstream enforcement keeps value copying mechanical and keeps ownership/ref bookkeeping confined to storage forms that participate in the memory model directly.

```zane
struct Vec2 {
    x Float
    y Float
}

struct Rect {
    pos Vec2
    size Vec2
}

struct BadOwner {
    engine Engine      // ILLEGAL: class field inside a struct
}

struct BadRef {
    target &Engine  // ILLEGAL: `&` field inside a struct
}
```

### 2.11 Symbols require direct initialization
Every symbol declaration **MUST** provide its initial value in the declaration itself. Zane does not permit bare symbol declarations followed by conditional or delayed first assignment.

```zane
text String   // ILLEGAL: symbols require direct initialization
```

```zane
text String = ""   // LEGAL: directly initialized
if runtimeBool() {
    text = "hi"
}
```

---

## 3. Scope Rules and Moves

### 3.1 `&` assignment uses owner scope
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

### 3.2 Only direct owning symbols are move-sources
A move-source must be a **direct owning symbol**: an owning local binding or owning parameter named directly by an identifier expression.

The following are **not** move-sources:
- an `&` value (refs are non-owning and cannot transfer ownership)
- a field access such as `car.engine`
- a container element access such as `cars[1]`
- any other access path, including method call results

```zane
engine Engine()
car Car(engine)          // legal: engine is a direct owning symbol

truck Truck(car.engine)  // ILLEGAL: field access is not a direct symbol
garage Garage(cars[1])   // ILLEGAL: container element is not a direct symbol
```

This rule makes containers stable ownership subtrees. Once a value is owned by a field or stored in a container element, it cannot be individually moved out. The containing object may be moved as a whole if it is itself a direct owning symbol.

### 3.3 Moves are restricted to the declaration block
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

### 3.4 Destination scope must contain or match source scope
A value may move into a new owner only when the destination owner is declared in the same or a higher lexical scope than the source owner.

```zane
node Node()
{
    nestedOwner Node()
    nestedOwner = node // ILLEGAL: cannot move into an owner declared in a nested scope
}
```

### 3.5 Callee-decides move semantics
When a value is passed to a function, the callee decides whether a move happens by what it does internally:

- storing the value in an owning slot moves it
- not storing it leaves ownership in place

For `&` fields specifically, the callee must declare the corresponding parameter as `&T` (§2.9). Attempting to bind a plain `T` parameter into `&` storage is a compile-time error. This means the callee's signature signals whether an `&`-creating source is required at the call site.

### 3.6 Moved symbols downgrade to `&` values and are no longer movable
After a direct owning symbol is moved, that symbol is downgraded to an `&` value through the anchor. The symbol remains readable but cannot be moved again.

```zane
engine Engine()
car Car(engine)          // engine is moved; downgrades to `&`
engine:inspect()         // legal: engine is now an `&`, still readable
truck Truck(engine)      // ILLEGAL: engine is an `&`, not a move-source
```

This also applies across calls: if a callee moves a caller-owned value, the caller can still read the symbol afterward through the downgraded `&`. Zane therefore has no user-visible use-after-move error class for reads.

### 3.7 Returned `&` values must be rooted in a parameter
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

## 4. Lifetime and Destruction

### 4.1 Destruction is deterministic
Class instances are destroyed when their owner dies, their owning container dies, or their owning scope drains under the concurrency rules.

### 4.2 Scopes drain before destruction
If a scope launches concurrent work, objects owned by that scope remain alive until all spawned work in that scope finishes. This is the water-tower rule.

### 4.3 Ref storage never extends lifetime
Refs do not participate in ownership and cannot prolong object lifetime. They only track a live object whose owner is already guaranteed to outlive them.

### 4.4 Null refs are not a user-facing state
Because scope rules prevent refs from outliving their owners, the runtime does not expose a normal “null ref” programming model to the user.

---

## 5. Memory Layout

### 5.1 Single reserved memory region
The runtime reserves one contiguous region at startup. The heap grows upward and the stack grows downward. If they meet, the program terminates with out-of-memory.

### 5.2 Heap allocation uses size-indexed free stacks
Heap allocations reuse previously freed slots by exact rounded size:

- requested sizes are rounded to 8-byte boundaries
- each rounded size has its own free stack
- allocation first pops from the matching free stack, then bumps the frontier if needed

This keeps allocation and free O(1) and avoids coalescing logic.

### 5.3 Struct and class layout follow declaration order
Fields are laid out in declaration order. Structs are stored inline. Class objects carry heap identity and anchor metadata as required by the implementation.

### 5.4 Booleans may be packed
The compiler may pack booleans in structs and stack frames when doing so does not change language semantics.

---

## 6. Anchors and Refs

### 6.1 Anchors are created lazily
Objects that are never ref'd pay no anchor-allocation cost. The first ref creation allocates and initializes the anchor.

### 6.2 Anchors are stable indirection points
A ref follows an owner/anchor path rather than pointing directly at an object. If an object moves or its owner slot is overwritten, the anchor path is updated and all refs observe the owner's current value.

### 6.3 Moves and overwrites update anchors, not all refs
Object relocation and owner overwrite are O(1) with respect to the number of refs, because only the anchor location record changes.

### 6.4 Destroying an object frees its anchor
When the owning object is destroyed, its anchor is torn down as part of destruction. Since refs cannot outlive the owner, this does not create a dangling-user-reference state.

### 6.5 Why refs never dangle
A dangling ref would require one of three failures: an owner overwrite breaking existing refs, a ref outliving the owner's scope, or an object move leaving refs pointed at the old address. Zane rules eliminate each case directly.

Owner/anchor indirection makes overwrite follow the current owner value instead of a dead object. The same-or-higher-scope rule keeps refs inside the owner's lifetime envelope. Anchor indirection turns object relocation into a single metadata update instead of a global ref rewrite. The model is therefore enforced by storage shape and lexical scope, not by runtime borrow tracking.

---

## 7. Language Comparisons

### 7.1 Ownership and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner by default | ✅ | ❌ | ❌ | ✅ |
| Non-owning refs as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Ref counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Refs remain usable across moves | ✅ via anchors | ❌ | ❌ | ⚠️ only when borrow checking permits the move pattern |
| Owner overwrite keeps existing refs valid | ✅ via owner/anchor indirection | ❌ | ❌ | ⚠️ heavily restricted by borrow checking |

### 7.2 Memory behavior

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Destruction timing | deterministic | non-deterministic | deterministic | manual / RAII |
| GC pauses | ❌ | ✅ | ❌ | ❌ |
| Allocation reuse | exact-size free stacks | runtime-managed | allocator-dependent | allocator-dependent |
| Dangling ref risk | ❌ | ❌ | ❌ | ✅ |
| Lifetime annotations | ❌ | ❌ | ✅ | ❌ |

---

## 8. Design Rationale

| Decision | Rationale |
|---|---|
| Overwritable owning storage | Owner/anchor indirection lets ordinary reassignment coexist with stable refs, including inside owning containers. |
| Structs remain overwritable | Structs are plain values; restricting reassignment would add complexity with no safety benefit. |
| Struct-downstream enforcement | Inline value copying must never duplicate ownership or ref-tracking state implicitly. |
| `&` in parameter and return positions | Allows ordinary APIs to accept and return refs without inventing a separate getter mechanism. |
| Rooted-source requirement for new `&` | A ref must come from an existing owner-rooted path. Binding to a temporary or plain value-only parameter would require ghost refs or compiler-invented storage, obscure object identity and lifetime, and contradict the definition of a non-owning reference. |
| No new `&` from `[]` | Prevents element-reference invalidation rules from leaking out of containers that own their elements. |
| Only direct owning symbols are move-sources | Makes containers stable ownership subtrees. Once a value is inside a field or container element, it cannot be individually extracted and re-parented. Ownership trees remain predictable and do not depend on runtime control flow. |
| Refs are never move-sources | Refs are non-owning aliases. Allowing moves from refs would enable re-parenting through alias paths and break the owner-uniqueness property. |
| Moves restricted to declaration block | Prevents conditional moves and flow-dependent ownership changes. A symbol's ownership transfer happens at most once, in a predictable location. This eliminates the need for flow-sensitive "maybe moved" analysis. |
| Moved symbols downgrade to refs | Moved-from symbols remain usable for reads through anchor-based ref downgrade. This preserves ergonomics while preventing double-moves and re-parenting. |
| Direct initialization for symbols | Eliminates maybe-uninitialized storage paths now that symbols may be reassigned later. |
| Lexical scope rules | A simple same-or-higher-scope rule is easy to implement and explain. |
| Callee-decides moves with caller downgrade | Preserves ownership while avoiding Rust-style use-after-move friction. |
| Lazy anchors | Objects with no refs do not pay ref-tracking costs. |
| Anchor indirection | Makes object moves O(1) with respect to the number of refs. |
| Shared size-indexed free stacks | Keeps allocation predictable and avoids general-purpose allocator overhead. |
| Water-tower destruction | Extends the same ownership model into concurrent execution without adding GC or async lifetimes. |

---

## 9. Summary

| Concept | Rule |
|---|---|
| Owning storage | Class-typed symbols, fields, and container elements are directly initialized and may later be overwritten |
| Struct value | May be overwritten freely |
| `&` | Non-owning storage; may be repointed, copied by value, and returned from functions |
| Place expression | Existing stable storage: a named symbol, a field access of a place, a place-projection subscript of a place, or an `&` parameter |
| New `&` value | May be initialized only from a named symbol, a field access of a place, or an `&` parameter; temporaries and `[]` expressions are rejected |
| `&` parameter | Declares that the caller must supply an `&`-creating source; the parameter is place-like inside the callee |
| Plain `T` parameter | Value-only binding; caller need not supply an `&`-creating source; MUST NOT be bound into `&` storage or returned as a new `&T` |
| Struct-downstream enforcement | Structs may contain only primitives and other structs, transitively |
| Symbol declaration | Must be directly initialized |
| `&` return | Returned `&T` must be rooted in a parameter; `this` counts |
| Ref assignment | Only from a place expression whose owner is in the same or a higher lexical scope than the ref |
| Move-source | Only a direct owning symbol (local or parameter); not an `&`, field, container element, or access path |
| Move declaration-block restriction | A direct owning symbol may only be moved in the exact lexical block where it was declared; parameters are treated as declared at function body top |
| Move destination scope | Destination owner must be in the same or a higher lexical scope than the source owner |
| Post-move downgrade | After a move, the source symbol downgrades to an `&` and remains readable but is no longer a move-source |
| Function call | Callee decides move; caller symbol remains usable |
| Anchor | Lazy, stable indirection for refs across moves |
| Destruction | Deterministic and delayed until the owning scope drains |
