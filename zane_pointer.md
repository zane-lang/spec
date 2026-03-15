# The Zane Pointer

## Overview

The Zane pointer is the core memory model of the Zane programming language. It enforces **single ownership** with **weak references**, structured as a strict **supervisor tree**. Every object on the heap has exactly one owner at all times. All other references are weak and cannot extend the object's lifetime.

There are no shared pointers, no garbage collector, no reference cycles, and no lifetime annotations. Destruction is deterministic and always propagates strictly downward through the ownership tree.

---

## Rules

### 1. Every object has exactly one strong reference
When an object is created, it is born with a single strong reference. There can never be more than one strong reference to the same object at any point in time.

### 2. Assignment copies a weak reference
Assigning a pointer to another variable always produces a weak reference. The original strong reference is not affected.

```zane
b = a       // b is weak, a remains strong
```

### 3. Ownership is declared in the type

Ownership transfer is expressed through the type system rather than a keyword. The `store` qualifier on a type parameter declares that the container is the strong owner of its elements. This applies to any generic container — lists, maps, or user-defined types.

```zane
List<store Tank>     // the list owns the Tanks — strong reference, data stored inline
List<Tank>           // the list holds references to heap-allocated Tanks — weak by default
```

When a `Tank` is placed into a `List<store Tank>`, ownership transfers to the list. When the list is destroyed, all Tanks it contains are destroyed with it. There is no separate transfer keyword — the type declaration is the contract.

For non-generic ownership — passing a strong reference to a function or storing it in a class field — the owner is determined by scope and field declaration in the same way. The strong reference lives exactly as long as its containing scope or class. Structs cannot hold class fields (struct-downstream enforcement).

### 4. When the strong reference goes out of scope, the object is destroyed
The object's destructor runs, its memory is returned to the heap, and **all weak references to it are immediately nulled** via its anchor. There is no dangling pointer risk — accessing a null weak reference is a caught error, not undefined behaviour.

### 5. Weak references do not extend lifetime
A weak reference going out of scope does nothing. Only the strong reference controls the object's lifetime.

### 6. The ownership structure is always a tree
Because only one strong reference can exist at a time, and a child cannot own its parent, ownership forms a strict tree. There are no cycles. When any node in the tree is destroyed, its entire subtree is destroyed with it.

---

## Weak references and anchors

Weak references never point directly at an object. They hold a `Stack<*anchor>` — a fixed-length sequence of absolute anchor addresses representing the chain of class-typed values traversed to reach the target. Every class instance on the heap has exactly one anchor: a separately allocated, fixed-address object that holds the instance's current heap offset. The anchor never moves. The object may.

```
weak_ref → Stack<*anchor> → anchors → object
```

This means moving an object — when a list grows and relocates its data, for example — requires updating exactly one location: the anchor's stored offset. Every weak ref becomes valid at the new address automatically, without touching the weak refs themselves.

When an object is destroyed, its anchor iterates its `weak_ref_stack` and nulls every registered weak ref. Every weak ref that subsequently dereferences receives null and triggers a caught error rather than accessing freed memory.

The length of the anchor stack is statically known from the type at the point the weak ref is created — it equals the number of class-typed levels crossed in the access expression. The compiler emits the stack as a fixed-length inline array. Struct fields are value types and contribute no anchor to the chain.

Anchors are created lazily — only when the first weak reference to an object is made. Every class instance carries an 8-byte back-pointer slot initialised to **0** at creation. When a weak ref is first created, the runtime allocates an anchor and writes its absolute address into the back-pointer slot. On destruction, a back-pointer of 0 means no anchor was ever created and the object is freed in a single operation; a non-zero back-pointer triggers anchor teardown before the object slot is freed.

0 is a safe sentinel because the Zane heap is mapped by `mmap` at a non-zero address — the OS guarantees that virtual address 0 is never mapped to any process. No real anchor can ever have the address 0.

```
weak_ref = { anchors: Stack<*anchor> }   // length known at compile time

dereference:
    addr = heap_base + anchors[0].offset  // outermost object — heap-relative
    for i in 1..anchors.len:
        addr = addr + anchors[i].offset   // each nested level — parent-relative
    return addr
```

Since elements are never reordered in a `List<store T>`, element anchor offsets are stable for the element's entire lifetime. When the list moves, only the list anchor's offset changes — all element anchors and all weak refs remain valid automatically.

---

## The `store` qualifier

`store` in a type parameter always means: the data lives here, not on the heap separately. This applies recursively across nested type parameters:

```zane
List<store Tank>
  // Tank data inline in the list — no pointer chase

List<store List<store Tank>>
  // inner List headers inline in the outer list
  // Tank data inline in each inner list
  // the whole structure is contiguous on the heap

List<store List<Tank>>
  // inner List headers inline in the outer list
  // Tanks live on the heap separately, accessed via pointer from each inner list
```

Structs were always `store` semantics — value types stored inline wherever they appear. `store` in type parameters extends this intuition explicitly to class types.

---

## Lifetime composition

Objects are composed by placing them into owning containers or scopes. Ownership transfers at the point of insertion:

```zane
let tanks: List<store Tank> = List()
tanks.push(Tank(...))   // Tank is now owned by the list
tanks.push(Tank(...))   // same
// when tanks goes out of scope, all Tanks are destroyed
```

For class composition, a field declared with a class type is the strong owner of that field's value for the class's lifetime. The compiler enforces that a value is not used after it has been transferred to an owner. Structs may only contain primitives and other structs — never class fields — so ownership of heap data always lives in a class.

---

## Destruction and the supervisor tree

When a strong reference dies — either by going out of scope or being replaced — the following happens in order:

1. The object's destructor runs.
2. The anchor iterates its weak ref stack and nulls every registered weak reference — all weak refs to the object become null immediately.
3. All objects the destroyed object owned (its children in the tree) are destroyed recursively by the same rules.

**Exception: return.** If a strong reference is returned from a function, the returning scope does not destroy it. Ownership transfers to the caller instead. The object's lifetime extends into whatever scope receives the return value. If the caller discards the return value, destruction happens there.

Destruction order is always deterministic and knowable at compile time. No graph traversal, no cycle detection, no GC pause.

---

## Error behaviour

Accessing a null weak reference — one whose strong owner has been destroyed — is a caught runtime error that terminates the offending branch cleanly. There is no silent undefined behaviour.

---

## Comparison

| Feature | Zane | C++ `unique_ptr` | C++ `shared_ptr` | Rust |
|---|---|---|---|---|
| Single owner | ✅ | ✅ | ❌ | ✅ |
| Weak refs nulled on destroy | ✅ | ❌ | via `weak_ptr` | ❌ |
| Ownership via type system | ✅ | ❌ | ❌ | ✅ |
| Inline element storage | `store` qualifier | ❌ | ❌ | manual |
| Ownership cycles possible | ❌ | ❌ | ✅ | ✅ |
| Lifetime annotations required | ❌ | ❌ | ❌ | ✅ |
| Ref counting | ❌ | ❌ | ✅ | via `Rc`/`Arc` |
| Garbage collector | ❌ | ❌ | ❌ | ❌ |
| Move safety (anchor) | ✅ | ❌ | ❌ | compile-time |
