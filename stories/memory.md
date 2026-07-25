# Stories: Memory Model

> **See also:** [`spec/memory.md`](../spec/memory.md) — the rules this story explains.

## Safety without a collector and without lifetimes

The starting commitment was a subtraction: no tracing garbage collector and no lifetime annotations. Zane keeps single hosting and deterministic, scope-driven destruction, but makes guest safety follow from the shape of storage and lexical scope rather than from runtime tracing or source-level lifetime parameters.

A host is the one storage location that controls a reference-type value's lifetime. A guest is non-hosting and can never extend that lifetime. The compiler checks that every guest remains inside the lifetime envelope of the host it follows. That rule is what makes deterministic teardown and immediate anchor reuse possible later in the design.

## The move problem, and the anchor that never moves

Single hosting wants values to move: into return slots, outer scopes, fields, and containers. A raw address cannot survive that. The solution is one fixed cell containing the value's current location. A guest stores the identity of that cell rather than the value's address. Moving or overwriting the value updates one cell, and every guest follows on its next access.

Updating guests directly was rejected because it would require enumerating them and make every move O(number of guests). The anchor reverses the cost: moves are O(1), while guest access pays one dependent cell load.

The cell must remain the same cell for the complete hosting lineage. If promotion moved the cell, every guest would need repointing. If promotion created another independent cell, later moves could update one path and stale the other. A stable global cell avoids both problems when at most one side of a move has live guests.

A move whose source and destination both already have live guests is rejected. Each guest set names a different stable identity, so preserving both would require forwarding or enumerating guests. When only one side has live guests, that side's cell becomes canonical; when neither does but the source slot becomes a readable moved-from guest, the runtime lazily allocates the one cell it now needs.

## Finding the anchor, and not paying when there are no guests

Every reference-type payload carries one small backpointer. `0` means no anchor exists. The first guest lazily allocates a cell, writes the payload's current segmented offset into it, and stores the cell's identity in the payload. Later guests copy that identity.

One backpointer is sufficient. The payload knows its anchor but never knows or enumerates its guests. Rehosting transfers the same backpointer to the destination and updates the one cell.

## Guest-only and host-capable guest storage

Not every guest can become a host. An explicitly declared `&T` contains only a tether and has no room for a `T`; it is permanently guest-only.

A slot declared as `T` is different. When its value is rehosted, the slot may remain readable as a guest to the new host, but the physical slot still has the full size and alignment of `T`. It is therefore a **host-capable guest**: it can later be overwritten with another `T` and become a host again.

This distinction does not change access syntax, but it matters to layout and assignment. Guest behavior describes how a slot reaches a value; host capability describes how much storage the slot owns.

## Where a new guest may come from

A new `&` may be created only from stable, host-rooted storage: a named symbol, a field of a place, or an existing `&` parameter. Temporaries are rejected because they have no lasting host. A subscript may read a guest already stored in a container, but it cannot mint a guest from a hosting element whose slot might later be removed or reused.

The restriction is intentionally structural. Instead of dynamically tracking arbitrary interior references, the language prevents unstable guest identities from being created.

## The value world stays closed, and placement stays the compiler's

Value types contain only value types and primitives, transitively. They carry no anchors and can be copied mechanically. Reference-type hosts, by contrast, carry identity and one backpointer, but their statically sized bytes may still sit inline beside values in the scope's fixed-size region.

Dynamic core types such as `List` and `String` keep fixed-size handles inline. Their variable-sized backing stores live separately, so growing a buffer cannot shift neighbouring hosts or values. Placement is unobservable: the compiler may optimize physical storage as long as destruction, hosting, and guest resolution remain unchanged.

## When the free stacks fragment, and the arena takes the scope

A single size-class allocator for every object was rejected because classes hoard memory from each other. Ordinary fixed-size scope storage does not need individual reuse: it can use a bump frontier and disappear in bulk when the scope drains.

Resizable backing stores are different. A list that grows abandons old buffers while the scope may remain active. Each scope therefore has a separate dynamic region with exact-size LIFO stacks. Allocation checks the corresponding size stack first and bumps the dynamic frontier only when that stack is empty. There is no borrowing from neighbouring classes and no coalescing.

Byte size, not element type, defines the classes. A new list starts with 128 bytes regardless of `T`; capacity is derived from `stride(T)`. This makes a returned block reusable by lists of other element types, strings, and other dynamic core values.

Growth doubles the byte size. The allocator first looks for a reusable doubled block. If none exists and the current ordinary block is the frontier allocation with room before its 1 MiB chunk boundary, it grows in place. Otherwise the elements move into a new doubled block and the old block enters its exact-size stack.

## The block larger than a page

Ordinary dynamic blocks never cross a 1 MiB chunk boundary. Once a power-of-two block exceeds 1 MiB, it becomes an **oversized span**: a dedicated contiguous OS mapping made from consecutive dynamic chunks.

The handle still stores one base segmented offset and one size class. Resolving the base produces a contiguous address range, so element indexing continues normally across the constituent chunks. Every chunk also has a directory entry. Oversized spans participate in the same exact-size reuse policy, but they are never extended in place; later growth relocates into a doubled span.

This preserves the simple segmented-offset handle without imposing a one-page maximum on lists and strings.

## The last table problem, and the segmented offset

A monolithic growable anchor array would eventually relocate, while native pointers to individually allocated cells would make every tether and backpointer 64 bits. Segmented `u32` offsets avoid both costs. The high bits select a 1 MiB chunk and the low bits select an aligned word inside it. A small chunk directory maps that identity to a native base.

Scope chunks and anchor pages use the same directory. Tethers, backpointers, payload locations, dynamic handles, and allocator free-stack entries therefore share one compact representation. Because the low bits count 8-byte words, each anchor uses an 8-byte-aligned physical slot: four bytes for its `u32` payload and four reserved bytes. A 1 MiB anchor page contains 131072 such slots.

The value `0` is reserved as “no anchor” wherever an anchor identity is expected. Payload offset zero remains valid; only the global anchor pool refuses to issue cell identity zero.

## Where the cells live, and the scan that pays for them

Interleaving anchors with payloads makes fixed-size scans less dense and lets guest-creation history disturb alignment. Anchors therefore live in anchor-only pages outside every scope arena.

The pool is runtime-global because the cell follows the complete hosting lineage, not whichever scope currently contains the payload. Promotion updates the cell's payload offset and keeps its identity unchanged. No forwarding cell, re-anchoring, or guest repointing is needed.

All anchor cells have the same physical size, so the pool has one free-address stack rather than size classes. Allocation pops that stack first and bumps the global frontier only when it is empty. Pages are mapped lazily and remain mapped until runtime shutdown, even when wholly free, so offsets retained by the stack never name unmapped or repurposed pages.

The global pool does require individual anchor teardown, but the host already supplies an exact teardown event. Overwriting an occupant keeps the hosting lineage alive and retains the cell. Rehosting transfers teardown responsibility to the destination. Only the end of the final hosting lineage returns the address to the stack.

Immediate reuse needs no generation counter. Lexical scope rules prove that no guest can outlive the host, including guests held by spawned work covered by the water-tower lifetime rules. When the hosting lineage ends, no live guest can still contain the old anchor identity. The ABA state is therefore unrepresentable rather than dynamically detected.

A concurrent implementation may use thread-local caches backed by the global pool to reduce contention, without changing the semantic identity or lifetime of any anchor.

## The sentinel that costs one reserved identity, and the buffer that wanted a line

The global pool reserves anchor identity zero for the untethered sentinel. That costs one unusable slot identity, not one page. Payloads can still occupy segmented offset zero because an anchor cell may legitimately contain that payload location.

Dynamic buffers have a separate alignment concern. Their region starts on chunk boundaries and allocates power-of-two blocks beginning at 128 bytes. Ordinary blocks, reused blocks, and oversized spans therefore remain cache-line aligned without forcing the same padding onto small inline values.

## Two vocabularies: host and guest above anchor and tether

The source-facing relationship is **host / guest**. A host stores the value and governs its lifetime. A guest may access the hosted value but neither stores it nor controls how long it lives.

The runtime-facing mechanism is **anchor / tether**. A guest is represented by a tether that names an anchor; the anchor records the payload's current location. Rehosting updates the anchor, so the guest keeps working.

The concise model is: *an object lives in a host; a guest may access it; internally, the guest's tether follows the object through its anchor.*
