"""Per-test metadata and chart colours for the Zane benchmark page.

Split out of runbench.py so the driver holds only the pipeline: the tables
here describe what each test *is*, which is editorial content that changes
independently of how the page is built.
"""

# ─────────────────────────────────────────────────────────────
# Test metadata: short name, title, setup, and per-impl facts.
# These track spec/memory.md: ownership is the default and the reference (&)
# is opt-in; a scope has a fixed-size region and a dynamic region that never
# share a chunk; an owner is settled (referenceable, never moves) or roaming
# (moves, referenced by nothing); and a reference is the u32 segmented offset
# of the settled owner it names. An owned object carries no metadata of its own.
#
# The "Reading the result" note for each test is NOT stored here. It is read
# from explanations.txt — result interpretation authored after looking at a
# real run — so it describes the measured numbers rather than predicting them.
# It renders as its own section below the chart, not as a caption on the panel.
# ─────────────────────────────────────────────────────────────

TEST_META = {
    "Test 1": {
        "short": "T1 — seq alloc+free",
        "title": "Sequential alloc then sequential free",
        "setup": "One fixed-region frontier bump per object, with a chunk-boundary check. An object carries no header, so allocation writes nothing into the object. Release is a no-op — the fixed-size region reclaims only when the scope drains. The arena row is a flat bump with no chunk boundary.",
        "meta": [
            ("Object size", "32B — no per-object metadata"),
            ("Alloc cost", "one fixed-region bump"),
            ("Release cost", "no-op — fixed region reclaims only at drain"),
            ("Arena row", "flat bump, no chunk boundary"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 2": {
        "short": "T2 — random-order free",
        "title": "Sequential alloc, then random-order release (only release timed)",
        "setup": "Alloc and shuffle untimed. The fixed-size region has no free list and no coalescing, so release is a no-op regardless of order.",
        "meta": [
            ("Object size", "32B"),
            ("Timed phase", "none — the release loop is a no-op the compiler removes"),
            ("Release path", "no-op — bulk reclaim at scope drain"),
        ],
    },
    "Test 3": {
        "short": "T3 — mixed sizes",
        "title": "Mixed-size alloc and random-order release",
        "setup": "Raw fixed-region blocks of four sizes, released in random order. The region is a pure bump: no size classes, no free list, no coalescing.",
        "meta": [
            ("Sizes", "8, 16, 32, 64 bytes — cycled evenly"),
            ("Count", "100,000 total (25k per size)"),
            ("Release order", "Fisher-Yates shuffle"),
        ],
    },
    "Test 4": {
        "short": "T4 — iteration",
        "title": "Iterating 100k entity objects — five layouts",
        "setup": "Five layouts iterated; no alloc or release inside the timed loop.",
        "meta": [
            ("Object type", "Entity { id: i64, x: f64, y: f64, hp: i32 }"),
            ("Object size", "32 bytes"),
            ("Spec analogue", "Array&lt;Entity, 100000&gt; — fixed-size inline storage"),
            ("CChunked", "64 elements × 32B = 2048B per chunk"),
            ("UList", "8 elements × 32B = 256B per chunk"),
            ("Measured op", "sum all hp fields (read-only scan)"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 5": {
        "short": "T5 — list growth",
        "title": "Growing a List backing store by appending 100k items",
        "setup": "The growth rules of memory.md §3.6: start at a 128-byte block, ask for double on exhaustion, check that size's exact-size stack first. In-place growth only at the dynamic frontier and only within the 1 MiB chunk; otherwise relocate.",
        "meta": [
            ("Element type", "Entity { id: i64, x: f64, y: f64, hp: i32 }"),
            ("First block", "128B — capacity floor(128 / 32) = 4 elements"),
            ("Growth", "13 in-place frontier doublings, 128B → 1 MiB"),
            ("Oversized spans", "2 relocations: 1→2 MiB and 2→4 MiB"),
            ("Alignment", "cache-line (64B) — a growable backing store"),
            ("Old blocks", "returned to the (size, 64) exact-size stacks"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 6": {
        "short": "T6 — reference access",
        "title": "Reference access via a segmented offset vs a direct pointer",
        "setup": "A reference is the u32 segmented offset of the settled owner it names (memory.md §4.1). Resolving it is a shift, a mask and one chunk-directory load, then the object itself. Nothing is allocated to mint a reference and nothing is recorded in the owner.",
        "meta": [
            ("Direct", "raw C pointer dereference — baseline"),
            ("Segmented offset, dir cached", "chunk directory hoisted; offset → object"),
            ("Segmented offset, dir reloaded", "chunk directory re-fetched per access"),
            ("Reference size", "u32 segmented offset — half a 64-bit pointer"),
            ("Reference cost", "4B — the reference itself; the owner stores nothing"),
            ("Asserted", "every reference resolves to the object it was minted from"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 7": {
        "short": "T7 — game loop",
        "title": "Simulated game loop: spawn, kill, and update entities each frame",
        "setup": "Each spawn is a fixed-region bump; each kill is a no-op release. These are statically sized objects in the fixed-size region, which reclaims in bulk at drain.",
        "meta": [
            ("Entity size", "32B"),
            ("Frame count", "500 frames"),
            ("Spawns/frame", "30 new entities"),
            ("Kills/frame", "20 oldest + hp-drained deaths"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 8": {
        "short": "T8 — particle system",
        "title": "Particle system: burst-spawn short-lifetime objects every frame",
        "setup": "Maximum churn. Every death is a no-op release from the fixed-size region.",
        "meta": [
            ("Particle size", "24B"),
            ("Frame count", "500 frames"),
            ("Spawns/frame", "60 particles"),
            ("Lifetime", "TTL = random 10–30 frames"),
            ("Concurrent variant", "Zane-only work-stealing update; threads pre-started before benchmarks"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 9": {
        "short": "T9 — fragmentation",
        "title": "Checkerboard fragmentation then refill — only refill timed",
        "setup": "Phases A and B untimed, phase C timed. Phase B's releases are no-ops, so the refill simply bumps the frontier past the dead space.",
        "meta": [
            ("Object size", "32B"),
            ("Phase A (prep)", "alloc 100k objects"),
            ("Phase B (prep)", "release every even-indexed"),
            ("Phase C (timed)", "alloc 50k new objects"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 10": {
        "short": "T10 — tree teardown",
        "title": "Cascade destruction — Zane vs malloc and pool",
        "setup": "A tree torn down by post-order DFS. Node payloads release as no-ops; each 128-byte child list goes back on its exact-size stack. A reference leaves nothing in its owner, so how many references a tree has cannot change its teardown.",
        "meta": [
            ("Tree size", "~4,000 nodes, branch 0–6"),
            ("Child lists", "128B dynamic blocks returned to the size stack"),
            ("Stack key", "resolved once — a child list's size and alignment are fixed"),
            ("malloc", "free(node) per node, coalescing on each"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 11": {
        "short": "T11 — stress test",
        "title": "Fragmentation stress: objects + lists, random spawn / push / kill cycles",
        "setup": "Entities are fixed-region objects whose release is a no-op; list backing stores are dynamic blocks that start at 128B, double, and return to their exact-size stacks. Both regions run at once here.",
        "meta": [
            ("Object size", "32B"),
            ("List blocks", "128 / 256 / 512B, cache-line aligned"),
            ("Cycles", "200 cycles"),
            ("Per cycle", "spawn + create lists + push + update + kill"),
            ("Concurrency", "not added — shared randomized mutation would distort the workload"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 12": {
        "short": "T12 — concurrent scan",
        "title": "Concurrent shard scan over four independent Array&lt;Entity, 25000&gt; workloads",
        "setup": "Four read-only shards of one owned inline array, summed either sequentially or on four worker threads. Each run asserts the aggregate matches the deterministic baseline.",
        "meta": [
            ("Workers", "4"),
            ("Shard size", "25,000 entities"),
            ("Total layout", "Array&lt;Entity, 100000&gt; split into 4 independent shards"),
            ("Scheduler", "persistent work-stealing pool pre-started and warmed before timed runs"),
            ("Correctness", "aggregate hp sum asserted every run"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 13": {
        "short": "T13 — dynamic churn",
        "title": "Dynamic-region block churn: exact-size stacks vs a pure frontier",
        "setup": "Repeated create-and-destroy of equal-size dynamic blocks. memory.md §3.2 gives the region one LIFO stack per (byte size, alignment): an allocation pops that stack and bumps the frontier only when it is empty. A boxed payload takes its key from its declared type; a growable backing store's size is a runtime value. The frontier-only row bypasses the stacks, which is what the fixed-size region does.",
        "meta": [
            ("Sizes", "128 / 256 / 512B, cache-line aligned"),
            ("Blocks", "2,000 per size per round"),
            ("Rounds", "10 — round 1 bumps, rounds 2-10 pop"),
            ("Reuse key", "(byte size, alignment) — never approximate"),
            ("Static vs runtime", "a boxed payload indexes its stack directly; a backing store looks its up"),
            ("Asserted", "a freed block is handed back for the same size and alignment, and withheld from a different one"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 14": {
        "short": "T14 — boxed members",
        "title": "Boxed members: roaming escape vs deep value copy",
        "setup": "A recursive tree whose members are boxed (adt.md §4): a fixed-size handle inline, the payload in the dynamic region at exactly the node size. A move within the scope that holds the blocks copies only the root's handles, so it is not timed. An escape out of that scope relocates every boxed descendant recursively and returns each old block (memory.md §3.5); nothing inside a roaming owner is referenced, so nothing else is updated. A value copy reallocates every payload so the two share no storage (§2.3); fresh construction builds each node in place and copies nothing.",
        "meta": [
            ("Tree", "complete binary, depth 12 — 8,191 nodes"),
            ("Owned node", "16B — value and two handles"),
            ("Value node", "16B — value and two handles"),
            ("Boxed payload", "exact node size, node alignment; no size class, no floor"),
            ("Stack key", "resolved once from the member's type, not per allocation"),
            ("Escape", "recursive relocation; old blocks returned to their exact-size stacks"),
            ("Deep copy", "recursive; source keeps its own storage"),
            ("Fresh construction", "built directly in the destination — no copy"),
            ("Asserted", "overwriting a settled boxed member, and the boxed member inside it, reuses both blocks; references to either keep their address and see the replacement"),
            ("Runs", "20 — median reported"),
        ],
    },
}

# Colour palette — assigned per impl name pattern
IMPL_COLORS = {
    "Zane":    "#7c6ff7",
    "Arena":   "#3aab76",
    "Pool":    "#c49a2a",
    "malloc":  "#e05a3a",
    "Direct":  "#4a9edd",
    "Inline":  "#3aab76",
    "UList":   "#3aab76",
    "CChunk":  "#c45a8a",
    "Pointer": "#e05a3a",
    "C reall": "#e05a3a",
}

# Second-level colour variants for Zane sub-variants
ZANE_VARIANTS = {
    "mmap":               "#7c6ff7",
    "in-place":           "#7c6ff7",
    "refill":             "#7c6ff7",
    "size stacks":        "#7c6ff7",
}

# Dynamic-region block kinds (T13)
DYN_COLORS = [
    ("boxed payload",   "#7c6ff7"),
    ("backing store",   "#b8a4ff"),
]

# Boxed-member operations (T14)
BOX_COLORS = [
    ("deep-copy value tree",                "#c45a8a"),
    ("construct fresh value tree",          "#3aab76"),
]


def get_color(impl_name):
    """Pick a colour based on the implementation name."""
    lower = impl_name.lower()

    for key, color in BOX_COLORS:
        if lower.startswith(key):
            return color
    for key, color in DYN_COLORS:
        if lower.startswith(key):
            return color
    if "zane" in lower or "segmented" in lower or "owned tree" in lower:
        for key, color in ZANE_VARIANTS.items():
            if key in lower:
                return color
        return "#7c6ff7"

    if lower.startswith("frontier bump"):
        return "#3aab76"

    for prefix, color in IMPL_COLORS.items():
        if impl_name.startswith(prefix):
            return color

    if lower.startswith("owned array shards, concurrent"):
        return "#7c6ff7"
    if lower.startswith("owned array shards, sequential"):
        return "#3aab76"
    if "sequential" in lower:
        return "#c49a2a"
    if "shuffled" in lower:
        return "#e05a3a"
    if "work-stealing" in lower or "concurrent" in lower:
        return "#7c6ff7"

    return "#6b7280"  # fallback grey
