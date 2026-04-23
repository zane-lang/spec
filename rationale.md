# Zane Design Rationale

This document collects the design decisions behind Zane's core features and explains why each one was made. It is a companion to the individual spec documents, which describe *what* the language does. This document explains *why*.

---

## 1. Memory Model

| Decision | Rationale |
|---|---|
| Single ownership by default | Eliminates ambiguity about which variable owns an object. The owner is always the variable that received the constructor call or the container/field that received it via ownership transfer. No annotation needed. |
| `ref` as explicit opt-in for non-owning references | Ownership is the safe, default case. Non-owning access is the exception that must be declared. This makes the uncommon case visible without burdening the common case. |
| Anchor indirection for refs | Direct pointers to heap objects become invalid after a move. Anchors give refs a stable fixed-address target to follow, enabling O(1) object moves that require only one write to the anchor rather than a scan of all refs. |
| Lazy anchor allocation (0-sentinel) | Objects that are never ref'd pay zero anchor overhead. The 8-byte back-pointer is initialised to 0 at construction — no allocation, no setup. An anchor is only created when the first `ref` to the object is made. |
| Leaf-only ref registration | Registering a ref with only the leaf anchor (not all ancestors) keeps ref creation and destruction O(1). Ancestor teardown recurses naturally through the ownership tree, nulling refs at each level as children are destroyed. |
| Free stack table indexed by size | Fragmentation is eliminated by matching freed slots exactly to future allocations of the same size. There are no unusable holes — a freed slot is immediately reusable by any allocation of the same declared size. |
| Unified heap for all allocation types | Class instances, list data, and anchors share the same heap and the same free stacks. A freed object slot of any size is reusable by any other heap allocation of the same size. No over-allocation to one type at the expense of another. |
| All sizes rounded to multiples of 8 | Ensures every allocation is naturally aligned for any field it could contain, and ensures freed slots are always reusable by any allocation of the same declared size. Maximum waste is 7 bytes per object. |
| Inline list element storage | Eliminates pointer chasing entirely. Iterating elements is a pure linear scan through contiguous memory. Stable element addresses also make refs to elements valid across list mutations as long as no element is removed. |
| Boolean packing in structs and stack frames | Booleans are typically 1 bit of information. Packing them costs a read-modify-write on writes (negligible on modern hardware) and eliminates byte-level padding between booleans. The programmer declares booleans normally; the compiler does the packing invisibly. |
| Struct-downstream enforcement | Structs are copied by flat `memcpy` — there is no destructor, no anchor, no heap interaction. Allowing class or ref fields inside a struct would silently duplicate strong or weak references, violating single ownership and the anchor registration invariant. Making this a compile-time error is the only safe option. |
| Destruction order: children before parent | The user destructor sees a fully live subtree (children are still alive when the destructor runs). After the destructor, cleanup proceeds strictly downward — no object is ever freed while a descendant still holds a live reference to it. |
| Fixed total region size chosen at startup | The mmap reservation is fixed. This eliminates the need for a general-purpose allocator (no `malloc`, no sbrk) and makes all memory addresses predictable from process start. The cost is that the reservation size is a deployment concern. |

---

## 2. Object-Oriented Model

| Decision | Rationale |
|---|---|
| Class body contains fields only | Keeps class definitions minimal and readable at a glance. All behavioral declarations live at package scope, making it easy to find all functions that operate on a type. |
| Constructors are standalone declarations (no `this`) | The object does not exist when the constructor runs. There is no partial initialization. The constructor's only job is to produce a fully initialized instance via `init{ }`. |
| `init{ }` requires every field | Forces explicit initialization of all fields. There is no default initialization — the compiler uses Control Flow Graph analysis to guarantee all paths assign every field, with zero runtime overhead. |
| Methods are free functions with `this` as first parameter | Methods have no special status. They are ordinary functions that happen to receive a named `this` parameter granting private field access and `:` call syntax. This keeps the language model flat and consistent. |
| Private fields use `_` prefix, not a keyword | Visibility is declared in the name, not separately. The compiler enforces `_`-prefixed fields as accessible only via `this` in the defining package. No `private`/`public` keywords; no annotations. |
| Overload identity is parameter types only | Names, `mut`, and return type do not affect overload identity. This keeps overload resolution simple, predictable, and free from ambiguity. |
| `mut` does not create a distinct overload | `mut` is a behavioral modifier for effect analysis, not a structural property of the type signature. A function that mutates and one that reads have the same call contract from the perspective of the caller's type checker. |
| Extension by any package | Methods are package-scope functions, so any package can define new behavior on imported types. The only restriction is field visibility — extension methods cannot access private (`_`-prefixed) fields. |
| Package `$` separator for function references | All package-scope functions — methods and free functions alike — are referenced with the same `Package$name` syntax. No special syntax for method references. `this` becomes an explicit first argument in the function type. |
| Instanceful package pattern | A package that defines a class of the same name provides both static utilities (via `Package$`) and an instantiable stateful object. This keeps the namespace clean without needing separate module and class hierarchies. |
| Scope isolation in constructors and methods | Constructor and method bodies cannot access package-level values directly. Any package-level state must be passed as an explicit parameter. This keeps construction deterministic and prevents hidden dependencies on package state. |

---

## 3. Effect Model

| Decision | Rationale |
|---|---|
| `mut` as the only effect modifier | One modifier covers all mutation. The compiler derives stronger purity levels (Total Pure, Read-Only Impure, Instance-Local Mutation, Full Impure) automatically from ownership structure, `ref` usage, and call graph. No `pure`, `readonly`, or effect lists for the user to maintain. |
| No `pure` keyword | Purity is an inferred property, not an annotation. The compiler knows when a function is Total Pure from the absence of `ref` reads, capability access, `mut` calls, and parameter-escaping writes. Annotating purity would add noise and require the compiler to verify annotations it could derive on its own. |
| Read-only by default | Every function and method is read-only unless marked `mut`. This makes mutation visible at the declaration site without requiring an annotation on every pure function. |
| Capabilities are ordinary objects | There is no ambient authority. Code can only perform I/O if it holds a capability object. Capabilities are passed explicitly (as parameters, constructor arguments, or ref fields), making all side-effectful paths traceable through the ownership tree. |
| Abortability is orthogonal to effects | A function can be Total Pure and still abort. `?` is a structural type modifier on the return signature; `mut` is a behavioral modifier on the declaration. Neither implies or restricts the other. |
| Allocation and destruction are Full Impure boundaries | Even a logically simple constructor affects allocator state. Treating construction and destruction as Full Impure boundaries ensures they are not reordered past observable side effects. |

---

## 4. Error Handling

| Decision | Rationale |
|---|---|
| `?` separates return and abort types | Creates visual and syntactic symmetry between declaration and call site. The primary path is on the left; the secondary path is on the right. |
| Abort type is structural, not behavioral | Changing the abort type changes the physical call contract between function and caller. It cannot be dropped implicitly — it must be handled or explicitly propagated. |
| `mut` is behavioral, not structural | A `mut` method is a strict superset of a non-`mut` method in terms of behavior. The abort type is independent of `mut`. |
| `resolve` is a distinct keyword | Prevents ambiguity between "exit this block with a value" and "exit this function with a value." `return` and `abort` always refer to the parent function. `resolve` always refers to the handler block. Every other language leaves this to convention or labeled blocks. |
| `Void` abort type instead of empty `?` | Acknowledges that failure is a real, explicit code path. Mirrors the meaning of `Void` as a primary return type. Makes it impossible to accidentally omit handling — the abort type is always visible in the signature. |
| No stored `T?E` values | Abortability is a control flow construct, not a data construct. No `Result`-like value is created unless stored explicitly as `Union<T,E>`. This means zero heap allocation and zero union storage at the call site — just a conditional jump. |
| No implicit default initialization | Variables must be consciously set. The compiler uses Control Flow Graph analysis to guarantee all paths initialize a variable before use, with zero runtime overhead. |
| Exhaustiveness checking on every `?` handler | Every path through a `?` handler block must `resolve`, `return`, or `abort`. Missing paths are compile-time errors. This makes it impossible to silently fall through without handling the abort. |

---

## 5. Dependency Management

| Decision | Rationale |
|---|---|
| No central registry | Eliminates a single point of failure and control. The URL is the identity — two packages from different hosts can share the same short name without conflict. |
| Binaries committed to repository | Binaries are part of the git tree and covered by the commit hash. They cannot be swapped without creating a new commit and moving the tag, which is detectable by the manifest's commit hash verification. |
| Commit hash recorded in manifest | Tags are mutable references; the commit hash is the actual trust anchor. A mismatch between the recorded hash and the resolved hash aborts the fetch with a security error. |
| Tag protection recommended | Prevents force-pushing a tag on the forge. Commit hash verification in the manifest catches it even without forge-level protection. |
| Plain git clone for fetching | Works on any git host without forge-specific APIs. No release asset infrastructure, no package registry API. Any host that speaks git works out of the box. |
| Exact version pinning | Deterministic builds. No surprise upgrades. The exact version is always explicit in the manifest. Two developers with the same manifest always link the same symbols. |
| Symbol prefixing at pull time | The `!` placeholder in compiled library symbols is replaced with the version tag at fetch time, not at compile time. This means library authors compile once; consumers never recompile the library. |
| Version embedded in symbol name | Multiple versions of the same library coexist in one binary as entirely distinct symbols. The linker sees no ambiguity. There is no implicit global resolution. |
| Global shared package cache | The same package version is never downloaded or prefixed more than once across all projects on the machine. Packages are stored at `~/.zane/packages/<url>/<version>/`. |
| Go-style URL and version path mangling | `/` as subdirectory separator mirrors the Go module cache. Capital letters are escaped with `!`. Illegal filesystem characters cause a pull-time error. |
| Manifest key enforcement | There is no way to bypass the manifest and reference a versioned symbol directly in source. Version strings stay out of source code entirely. |
| Source-compile opt-in (`--from-source`) | Trust escape hatch for security-conscious consumers who do not trust the pre-built artifacts. Not a normal workflow. |
| Transitive deps auto-installed | The consumer does not need to enumerate indirect dependencies. The toolchain reads each library's `zane.coda` recursively and installs all transitive dependencies before the top-level package is considered ready. |
| Per-library version namespace | Diamond dependency conflicts resolve trivially. Different versions of the same library produce distinct symbol namespaces. No global version negotiation algorithm needed. |
