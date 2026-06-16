# Zane Memory Model

This document specifies Zane's memory model: ownership, refs, anchors, and heap layout. Lexical lifetime rules, ownership moves, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for classes and structs. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling references by combining single ownership, lexical lifetime rules, and anchor-based tracking of non-owning references.

- **`Overwritable owners`.** A class owner is directly initialized and may later be overwritten.
- **`Repointable refs`.** An `&` value is non-owning storage that can point at different owners over time.
- **`Lexical lifetime enforcement`.** Ref assignment and ownership moves are checked using declaration scope alone (see [`lifetimes.md`](lifetimes.md)).
- **`Deterministic destruction`.** Objects are destroyed when their owning scope drains; there is no tracing garbage collector (see [`lifetimes.md`](lifetimes.md) §2).

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
owners Array<Node, 2> = [Node(), Node()]
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
An `&` symbol or `&` field may be assigned a different target later, as long as the scope rule in [`lifetimes.md`](lifetimes.md) §1.1 is satisfied.

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
A parameter declared as `&T` requires the caller to supply a source that may create a new `&` under §2.8. Inside the callee body it acts as a place expression and may be stored into `&` storage or returned as `&T` under [`lifetimes.md`](lifetimes.md) §1.7.

A parameter declared as plain `T` is a **value-only binding**. The caller is not required to supply a place expression. A plain `T` parameter does not guarantee a stable `&`-rootable source location, therefore it **MUST NOT** be bound into `&` storage or returned as a new `&T`. Inside the callee body, a plain `T` parameter is not a place expression for `&`-binding purposes.

```zane
type Car = class {
    engine &Engine;
    _value Int;
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
type Vec2 = struct {
    x Float;
    y Float;
}

type Rect = struct {
    pos Vec2;
    size Vec2;
}

type BadOwner = struct {
    engine Engine;      // ILLEGAL: class field inside a struct
}

type BadRef = struct {
    target &Engine;  // ILLEGAL: `&` field inside a struct
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

## 3. Memory Layout

### 3.1 Single reserved memory region
The runtime reserves one contiguous region at startup. The heap grows upward and the stack grows downward. If they meet, the program terminates with out-of-memory.

### 3.2 Heap allocation uses size-indexed free stacks
Heap allocations reuse previously freed slots by exact rounded size:

- requested sizes are rounded to 8-byte boundaries
- each rounded size has its own free stack
- allocation first pops from the matching free stack, then bumps the frontier if needed

This keeps allocation and free O(1) and avoids coalescing logic.

### 3.3 Struct and class layout follow declaration order
Fields are laid out in declaration order. Structs are stored inline. Class objects carry heap identity and anchor metadata as required by the implementation.

### 3.4 Booleans may be packed
The compiler may pack booleans in structs and stack frames when doing so does not change language semantics.

---

## 4. Anchors and Refs

### 4.1 Anchors are created lazily
Objects that are never ref'd pay no anchor-allocation cost. The first ref creation allocates and initializes the anchor.

### 4.2 Anchors are stable indirection points
A ref follows an owner/anchor path rather than pointing directly at an object. If an object moves or its owner slot is overwritten, the anchor path is updated and all refs observe the owner's current value.

### 4.3 Moves and overwrites update anchors, not all refs
Object relocation and owner overwrite are O(1) with respect to the number of refs, because only the anchor location record changes.

### 4.4 Destroying an object frees its anchor
When the owning object is destroyed, its anchor is torn down as part of destruction. Since refs cannot outlive the owner, this does not create a dangling-user-reference state.

### 4.5 Why refs never dangle
A dangling ref would require one of three failures: an owner overwrite breaking existing refs, a ref outliving the owner's scope, or an object move leaving refs pointed at the old address. Zane rules eliminate each case directly.

Owner/anchor indirection makes overwrite follow the current owner value instead of a dead object. The same-or-higher-scope rule keeps refs inside the owner's lifetime envelope. Anchor indirection turns object relocation into a single metadata update instead of a global ref rewrite. The model is therefore enforced by storage shape and lexical scope, not by runtime borrow tracking.

---

## 5. Language Comparisons

### 5.1 Ownership and references

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner by default | ✅ | ❌ | ❌ | ✅ |
| Non-owning refs as explicit opt-in | ✅ | ⚠️ Raw pointers | ⚠️ `weak_ptr` | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Ref counting required | ❌ | ❌ | ✅ | ⚠️ `Rc`/`Arc` only |
| Refs remain usable across moves | ✅ via anchors | ❌ | ❌ | ⚠️ only when borrow checking permits the move pattern |
| Owner overwrite keeps existing refs valid | ✅ via owner/anchor indirection | ❌ | ❌ | ⚠️ heavily restricted by borrow checking |

### 5.2 Allocation

| Property | Zane | GC languages | Rust | C/C++ |
|---|---|---|---|---|
| Allocation reuse | exact-size free stacks | runtime-managed | allocator-dependent | allocator-dependent |

> **See also:** [`lifetimes.md`](lifetimes.md) §3 for the lifetime and destruction behavior comparison.

---

## 6. Design Rationale

| Decision | Rationale |
|---|---|
| Overwritable owning storage | Owner/anchor indirection lets ordinary reassignment coexist with stable refs, including inside owning containers. |
| Structs remain overwritable | Structs are plain values; restricting reassignment would add complexity with no safety benefit. |
| Struct-downstream enforcement | Inline value copying must never duplicate ownership or ref-tracking state implicitly. |
| `&` in parameter and return positions | Allows ordinary APIs to accept and return refs without inventing a separate getter mechanism. |
| Rooted-source requirement for new `&` | A ref must come from an existing owner-rooted path. Binding to a temporary or plain value-only parameter would require ghost refs or compiler-invented storage, obscure object identity and lifetime, and contradict the definition of a non-owning reference. |
| No new `&` from `[]` | Prevents element-reference invalidation rules from leaking out of containers that own their elements. |
| Direct initialization for symbols | Eliminates maybe-uninitialized storage paths now that symbols may be reassigned later. |
| Lazy anchors | Objects with no refs do not pay ref-tracking costs. |
| Anchor indirection | Makes object moves O(1) with respect to the number of refs. |
| Shared size-indexed free stacks | Keeps allocation predictable and avoids general-purpose allocator overhead. |

---

## 7. Summary

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
| Anchor | Lazy, stable indirection for refs across moves |

> **See also:** [`lifetimes.md`](lifetimes.md) §5 for the summary of scope, move, and destruction rules.
