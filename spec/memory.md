# Zane Memory Model

This document specifies Zane's memory model: ownership, refs, anchors, and heap layout. Lexical lifetime rules, ownership moves, and deterministic destruction are specified in [`lifetimes.md`](lifetimes.md).

> **See also:** [`lifetimes.md`](lifetimes.md) for scope rules, moves, and destruction. [`types.md`](types.md) §2 for classes and structs. [`effects.md`](effects.md) §2 for `mut`. [`concurrency.md`](concurrency.md) §4 for water-tower lifetimes. [`syntax.md`](syntax.md) §1 and §2 for storage forms.

---

## 1. Overview

Zane eliminates dangling references by combining single ownership, lexical lifetime rules, and anchor-based tracking of non-owning references.

- **`Overwritable owners`.** A class owner is directly initialized and may later be overwritten.
- **`Repointable refs`.** An `&` value is non-owning storage that can point at different owners over time.
- **`Lexical lifetime enforcement`.** Ref assignment and ownership moves are checked using declaration scope alone (see [`lifetimes.md`](lifetimes.md) §1).
- **`Deterministic destruction`.** Objects are destroyed when their owning scope drains; there is no tracing garbage collector (see [`lifetimes.md`](lifetimes.md) §2).
- **`Stack-first placement`.** A class instance lives on the stack unless its size is dynamic or it escapes its creating frame; only dynamically-sized data is forced onto the heap (see §3.5).
- **`Index-form refs`.** An `&` value is a small integer index into a heap-resident anchor table, not a raw pointer (see §4.2).

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
The runtime reserves one contiguous region at startup, divided into three parts:

```
[anchor_ptr][ heap → … ← stack ]
```

- **`anchor_ptr`** is a single fixed-location word at the base of the region. It holds the current address of the anchor table (§4.1).
- The **heap** grows upward, starting just above `anchor_ptr`.
- The **stack** grows downward from the top.

The heap and stack grow toward each other; if they meet, the program terminates with out-of-memory. The anchor table is ordinary heap data reached through `anchor_ptr`, so only one fixed word is reserved up front — the table itself grows on demand (§4.1).

### 3.2 Heap allocation uses size-indexed free stacks
Heap allocations reuse previously freed slots by exact rounded size:

- requested sizes are rounded to 8-byte boundaries
- each rounded size has its own free stack
- allocation first pops from the matching free stack, then bumps the frontier if needed

This keeps allocation and free O(1) and avoids coalescing logic.

### 3.3 Struct and class layout follow declaration order
Fields are laid out in declaration order. Structs are stored inline. A class instance has stable identity and, once first referenced, carries one backpointer slot of anchor metadata (§4.2); an unreferenced instance carries none. Placement of a class instance — stack or heap — is covered in §3.5.

### 3.4 Booleans may be packed
The compiler may pack booleans in structs and stack frames when doing so does not change language semantics.

### 3.5 Class instances may be placed on the stack
Placement is an implementation decision, not a language-visible property. The compiler **MAY** place a class instance on the stack when both hold:

- its size is statically known, and
- it does not escape the frame that creates it in a way a move cannot satisfy.

A class instance is forced onto the heap only when its size is dynamic or it must outlive its creating frame through a longer-lived owner. Placement never changes observable semantics: destruction stays deterministic (see [`lifetimes.md`](lifetimes.md) §2), and refs resolve identically regardless of where the instance lives (§4). This freedom mirrors the boolean-packing rule (§3.4): the compiler may choose the cheaper placement whenever doing so cannot change program meaning.

### 3.6 Handle-typed core classes have fixed footprint
The core dynamically-sized classes — `List`, `String`, and their kin — are represented as a fixed-size **handle**: a small header (or single pointer) whose dynamic backing store lives on the heap. The handle occupies a statically known footprint wherever it is stored.

A type that contains a handle-typed field therefore stays statically sized. A class holding a `List` field does not become dynamically sized; it stores the fixed handle inline, and only the backing store behind the handle lives on the heap.

```zane
type Inventory = class {
    items List<Item>;   // fixed-size handle inline; backing store on the heap
    count Int;
}
```

This is what keeps stack placement (§3.5) broadly applicable: almost every value is statically sized at its own level, so dynamic size — the one unavoidable heap trigger — appears only inside the backing stores of handle types.

### 3.7 Moving a value reuses the destination slot
A move transfers ownership into a destination owner of the **same type** (see [`lifetimes.md`](lifetimes.md) §1). Because both sides have identical, statically known size, a move is a fixed-size overwrite of the destination slot:

- Moving into a fresh declaration or a return slot is in-place initialization.
- Moving into an already-initialized owner first destroys the current occupant, then overwrites the same-size slot.

Moves only ever target the same or a higher scope ([`lifetimes.md`](lifetimes.md) §1.4), so the destination always outlives the source and its slot already exists. Because handle-typed fields (§3.6) keep the moved footprint small, a move relocates only the inline bytes — a handle's backing store never moves. If the moved value is referenced, the move also updates its one anchor cell (§4.5), never the refs themselves.

---

## 4. Anchors and Refs

### 4.1 The anchor table
Refs are tracked through a single **anchor table**: a heap-resident array of fixed-size cells, each holding the current address of one referenced owner. The table is reached through `anchor_ptr` (§3.1), the one fixed word at the base of the memory region.

Each cell is one machine word — just the owner's current address; it stores nothing else. Because every cell is the same size, the table is the simplest possible pool: a bump frontier plus a free list, with no size classes and no coalescing. It is ordinary heap data, so it grows on demand; growing it allocates a larger block, copies the cells, and updates `anchor_ptr`. Growth touches only that one root word, never the refs (§4.2), so it is O(1) with respect to the number of live refs.

### 4.2 Refs are slot indices, not pointers
An `&` value is a **`u32` index** into the anchor table, not a raw pointer. A `u32` indexes over four billion simultaneously-referenced owners — far beyond any realistic working set, since only referenced class instances consume a slot — while keeping an `&` half the size of a 64-bit pointer.

Indices are **1-based**, and the value `0` means *unreferenced*:

- Physical slot `0` is reserved as a null/trap cell and is never handed out. Anchors start at slot `1`.
- A `u32` of `0` therefore reads as "no anchor yet," which is the natural state of zero-initialized storage.

Each referenced owner also stores its own slot index in a **`u32` backpointer**, allocated lazily with the slot. The backpointer lets the owner mint new refs — `&x` copies the index — and lets a move locate the owner's cell (§4.5). It is a single index, not a list of refs: the owner never enumerates the refs that point at it, which is what keeps moves O(1) (§4.5).

### 4.3 Anchors are created lazily
An owner that is never referenced pays nothing: its backpointer stays `0` and it consumes no table slot. The first `&` taken on an owner allocates a slot — popped from the free list, or bumped from the frontier if the free list is empty — writes the owner's address into the cell, and records the 1-based slot index in the owner's backpointer. Every subsequent `&` of that owner copies the index.

### 4.4 Dereferencing a ref
Resolving an `&` reads the table base from `anchor_ptr`, indexes to the cell, reads the owner's current address, then accesses the object. The reserved slot `0` means a dereference never underflows the table.

```
  &x  =  index n  (u32)
            │
            │   anchor_ptr ──▶ base of table
            ▼
  ┌──────────────────── anchor table (heap) ────────────────────┐
  │  slot 0  │  slot 1  │  slot 2  │   …   │   slot n            │
  │  (null)  │  addr    │  addr    │       │   addr ──┐          │
  └────────────────────────────────────────────────┼───────────┘
                                                     │  owner's
                                                     ▼  current address
                                               ┌───────────┐
                                               │  owner x  │   (stack or heap)
                                               │  fields…  │
                                               └───────────┘
```

The address arithmetic — the 1-based offset and the cell stride — folds into a single machine addressing mode (`base + n·stride`), so the index costs no extra instruction over a raw-pointer dereference. The added cost is one dependent load: the cell read between the index and the object. See §4.8.

### 4.5 Moves and overwrites update one cell, not all refs
A ref follows the owner/anchor path rather than pointing at a fixed object address. When an owner moves (§3.7) or an owning slot is overwritten, the runtime writes the owner's new address into its one anchor cell, located through the backpointer (§4.2). Every ref reads that cell on its next dereference, so all refs observe the owner's current value with no per-ref fixup.

This is why object relocation and owner overwrite are **O(1) with respect to the number of refs**. It is also how a moved-from symbol stays readable: after a move the symbol downgrades to an `&` — a slot index — and reads resolve through the cell to the value's new home (see [`lifetimes.md`](lifetimes.md) §1.6).

### 4.6 Destroying an owner frees its slot
When a referenced owner is destroyed, its anchor slot is returned to the free list as part of destruction. Freed cells thread the free list through themselves: a free cell stores the next free index, and the table header holds the free-list head (`0` when empty), so reuse costs no extra space.

Because scope rules keep every ref inside its owner's lifetime ([`lifetimes.md`](lifetimes.md) §1.1, §1.4), no live ref can point at a freed slot. Destruction therefore creates no dangling-reference state.

### 4.7 Why refs never dangle
A dangling ref would require one of three failures: an owner overwrite breaking existing refs, a ref outliving the owner's scope, or an object move leaving refs pointed at a dead address. The model eliminates each. Owner/anchor indirection makes overwrite and move follow the current cell value instead of a stale address (§4.5). The same-or-higher-scope rule keeps every ref inside the owner's lifetime envelope ([`lifetimes.md`](lifetimes.md) §1.1). The model is enforced by storage shape and lexical scope, not by runtime borrow tracking.

### 4.8 Resolution cost
The index encoding adds no arithmetic cost: the 1-based offset and cell stride fold into the machine addressing mode, and `anchor_ptr` is the hottest word in the program — pinned in a register across a region and reloaded only after a (rare) table growth.

The genuine cost of any anchor scheme is **one extra dependent load per ref dereference** — the cell read — versus an idealized raw pointer that cannot survive moves. That load is a few cycles when the cell is cache-warm, which it usually is: cells are one word each and the table is small and hot. It is paid only when dereferencing an `&`; direct access to an owner never consults the table. Across a run of accesses through the same ref with no intervening move or overwrite, the compiler resolves the owner address once and reuses it, so hot loops do not re-pay the load.

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
| Stack-first placement | A class needs the heap only for dynamic size or escape; placing static, non-escaping instances on the stack is cheaper and never changes observable behavior. |
| Handle-typed core classes | Representing `List`/`String` as fixed-size handles keeps containing types statically sized, so dynamic size — the one hard heap trigger — stays confined to backing stores. |
| Index-form refs (`u32`) | A small index is half a pointer's size, is immune to table relocation, and lets the table grow with a single root update; the index math folds into the addressing mode. |
| Single `anchor_ptr` root | Reserving one fixed word instead of a whole region defers all anchor storage to the heap, so the table grows on demand with no fixed cap to size. |
| Reserved 1-based slot 0 | A reserved null cell makes `0` mean "unreferenced" for free on zero-initialized storage and turns a stray sentinel dereference into a defined trap rather than an underflow. |
| Per-owner backpointer | Storing one slot index in the owner lets moves locate the cell and lets `&x` mint refs, without the owner ever enumerating its refs — preserving O(1) moves. |
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
| Class placement | Stack when statically sized and non-escaping; heap only for dynamic size or escape — an unobservable choice |
| `&` representation | A `u32` 1-based index into the heap anchor table; `0` means unreferenced |
| Anchor table | Heap-resident, rooted at the fixed `anchor_ptr`; one word per cell; grows on demand |
| Backpointer | Each referenced owner stores its own `u32` slot index for move updates and ref minting |
| Anchor lifecycle | Lazily allocated on first ref; freed to a cell-threaded free list on destruction |

> **See also:** [`lifetimes.md`](lifetimes.md) §5 for the summary of scope, move, and destruction rules.
