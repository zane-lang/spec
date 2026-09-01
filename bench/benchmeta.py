"""Per-test metadata and chart colours for the Zane benchmark page.

Split out of runbench.py so the driver holds only the pipeline: the tables
here describe what each test *is*, which is editorial content that changes
independently of how the page is built.
"""

# ─────────────────────────────────────────────────────────────
# Test metadata: short name, title, setup, and per-impl facts.
# These track spec/memory.md: hosting is the default and the guest (&) is
# opt-in; a scope owns a fixed-size region and a dynamic region that never
# share a chunk; anchors live in one runtime-global pool of 8-byte cells; and
# a guest is represented internally by a u32 tether — a segmented offset to an
# anchor cell that either terminates at a payload or forwards to another
# anchor. Each hosted payload stores a u32 backpointer to its terminal cell.
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
        "setup": "One fixed-region frontier bump plus a 4-byte backpointer zeroed to 0, so no anchor exists. Release is a no-op — the fixed-size region reclaims only when the scope drains. Two arena rows bracket the result: a pure bump that never touches the memory it hands out, and the same bump writing one 4-byte field per object at Zane's 40-byte stride.",
        "meta": [
            ("Object size", "32B + 4B backpointer"),
            ("Backpointer init", "0 — no anchor at creation"),
            ("Alloc cost", "one fixed-region bump + zero-write (touches every object)"),
            ("Release cost", "no-op — fixed region reclaims only at drain"),
            ("Anchor created", "never — no guests in this test"),
            ("Arena rows", "pure bump vs. the same bump initializing one field"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 2": {
        "short": "T2 — random-order free",
        "title": "Sequential alloc, then random-order release (only release timed)",
        "setup": "Alloc and shuffle untimed. The fixed-size region has no free list and no coalescing, so release is a no-op regardless of order.",
        "meta": [
            ("Object size", "40B runtime"),
            ("Backpointer", "0 — no guest taken"),
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
            ("Note", "raw blocks — no backpointer slot"),
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
        "short": "T6 — guest access",
        "title": "Guest access via a segmented tether vs a direct pointer",
        "setup": "Terminal resolution: a tether is a u32 segmented offset naming a cell in the global anchor pool, and the cell holds the payload's offset plus a word marking it a payload anchor rather than a forwarder. Both hops resolve through the chunk directory; anchor pages are their own chunks.",
        "meta": [
            ("Direct", "raw C pointer dereference — baseline"),
            ("Segmented tether, dir cached", "chunk directory hoisted; cell load → payload"),
            ("Segmented tether, dir reloaded", "chunk directory re-fetched per access"),
            ("Tether size", "u32 segmented offset — half a 64-bit pointer"),
            ("Anchor cell", "8B slot: u32 target + u32 payload/forwarding kind"),
            ("Tethered cost", "16B minimum: 4B tether + 8B cell + 4B backpointer"),
            ("Identity 0", "reserved by the pool as the untethered sentinel"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 7": {
        "short": "T7 — game loop",
        "title": "Simulated game loop: spawn, kill, and update entities each frame",
        "setup": "Each spawn zeroes a backpointer; each kill is a no-op release. These are statically sized hosts in the fixed-size region, which reclaims in bulk at drain.",
        "meta": [
            ("Entity size", "32B + 4B backpointer"),
            ("Anchor", "never created — no guests"),
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
            ("Particle size", "24B + 4B backpointer"),
            ("Anchor", "never created — no guests"),
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
            ("Object size", "32B + 4B backpointer"),
            ("Anchor", "never created"),
            ("Phase A (prep)", "alloc 100k objects"),
            ("Phase B (prep)", "release every even-indexed"),
            ("Phase C (timed)", "alloc 50k new objects"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 10": {
        "short": "T10 — tree teardown",
        "title": "Cascade destruction — three Zane guest densities vs malloc and pool",
        "setup": "Three Zane variants — no guests, a single root guest, one guest per node — all torn down by post-order DFS. Node payloads release as no-ops; each 128-byte child list goes back on its exact-size stack.",
        "meta": [
            ("Tree size", "~4,000 nodes, branch 0–6"),
            ("No guests", "backpointer = 0; no anchor allocated"),
            ("Single root guest", "one anchor; 3,999 nodes unanchored"),
            ("One guest per node", "every node mints its own anchor cell"),
            ("Child lists", "128B dynamic blocks returned to the size stack"),
            ("Stack key", "resolved once — a child list's size and alignment are fixed"),
            ("malloc", "free(node) per node, coalescing on each"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 11": {
        "short": "T11 — stress test",
        "title": "Fragmentation stress: hosts + lists, random spawn / push / kill cycles",
        "setup": "Entities are fixed-region hosts whose release is a no-op; list backing stores are dynamic blocks that start at 128B, double, and return to their exact-size stacks. Both regions run at once here.",
        "meta": [
            ("Object size", "32B + 4B backpointer"),
            ("List blocks", "128 / 256 / 512B, cache-line aligned"),
            ("Anchor", "never created — no guests"),
            ("Cycles", "200 cycles"),
            ("Per cycle", "spawn + create lists + push + update + kill"),
            ("Concurrency", "not added — shared randomized mutation would distort the workload"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 12": {
        "short": "T12 — concurrent scan",
        "title": "Concurrent shard scan over four independent Array&lt;Entity, 25000&gt; workloads",
        "setup": "Four read-only shards of one hosted inline array, summed either sequentially or on four worker threads. Each run asserts the aggregate matches the deterministic baseline.",
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
        "short": "T13 — partial-guest scan",
        "title": "Partial-guest repeated payload scan (anchor placement A/B)",
        "setup": "100k hosts, ~20% guested, then repeated payload-only field scans. By default the anchor pool owns its own chunks and the payload stream stays dense; -DZM_INTERLEAVE puts the cells beside the payloads so every scan drags them through cache.",
        "meta": [
            ("Hosts", "100,000"),
            ("Guested", "~20% (every 5th)"),
            ("Passes", "8 per timed run"),
            ("A/B", "default = global anchor pool; -DZM_INTERLEAVE = cells beside payloads"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 14": {
        "short": "T14 — scan-heavy mixed",
        "title": "Scan-heavy mixed workload, 10 scans : 1 resolve (anchor placement A/B)",
        "setup": "Ten payload scans per one tether-resolve pass. Models the weighting the 1:1 micro-tests hide: the pervasive scan should dominate, even though the pool's individual resolve is dearer than the interleaved layout's.",
        "meta": [
            ("Hosts", "100,000"),
            ("Guested", "~20% (every 5th)"),
            ("Ratio", "10 payload scans : 1 resolve pass"),
            ("A/B", "default = global anchor pool; -DZM_INTERLEAVE = cells beside payloads"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 15": {
        "short": "T15 — forwarding hops",
        "title": "Guest resolution across forwarding anchors",
        "setup": "Moving into an already-anchored destination keeps that cell terminal and turns the source into a forwarder (memory.md §4.5), so a guest minted before the move gains one hop per move it survived. The timed loop resolves the oldest tether. Two rows separate hop count from cache footprint: one compresses as it resolves, the other resolves a terminal identity over the identical structure — the floor the first converges on.",
        "meta": [
            ("Chains", "20,000"),
            ("Passes", "8 per timed run"),
            ("Hop cost", "one dependent anchor-cell load per uncompressed hop"),
            ("Chain build", "depth+1 hosts, each guested before the move"),
            ("Path compression", "first pass walks and rewrites the tether; later passes find it terminal and store it back unchanged"),
            ("Footprint floor", "terminal resolve over the same 4-hop structure"),
            ("Asserted", "a compressed tether equals the chain's terminal identity"),
            ("Cell accounting", "a depth-4 chain allocates 5 cells for 4 forwarding edges — merging two anchored identities allocates none"),
            ("Retirement", "forwarders return to the pool free stack when the source scope drains"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 16": {
        "short": "T16 — dynamic churn",
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
    "Test 17": {
        "short": "T17 — boxed members",
        "title": "Boxed members: rehost relocation vs deep value copy",
        "setup": "A recursive tree whose members are boxed (adt.md §4): a fixed-size handle inline, the payload in the dynamic region at exactly the node size. Rehosting the root relocates every boxed descendant recursively (memory.md §3.5), retargeting each contained host's anchor; a value copy reallocates every payload so the two share no storage (§2.3); fresh construction builds each node in place and copies nothing.",
        "meta": [
            ("Tree", "complete binary, depth 12 — 8,191 nodes"),
            ("Hosted node", "24B — value, two handles, and a 4B backpointer"),
            ("Value node", "16B — value and two handles, no identity"),
            ("Boxed payload", "exact node size, node alignment; no size class, no floor"),
            ("Stack key", "resolved once from the member's type, not per allocation"),
            ("Relocation", "recursive; old blocks returned to their exact-size stacks"),
            ("Deep copy", "recursive; source keeps its own storage"),
            ("Fresh construction", "built directly in the destination — no copy"),
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
    "Anchor":  "#7c6ff7",
    "Full":    "#b8a4ff",
}

# Second-level colour variants for Zane sub-variants
ZANE_VARIANTS = {
    "no guests":          "#7c6ff7",
    "single root":        "#5a4faa",
    "one guest per node": "#b8a4ff",
    "lazy anchors":       "#7c6ff7",
    "mmap":               "#7c6ff7",
    "in-place":           "#7c6ff7",
    "refill":             "#7c6ff7",
    "size stacks":        "#7c6ff7",
}

# Forwarding-hop depth (T15) — darker with each hop
HOP_COLORS = [
    ("terminal anchor (0 hops)",     "#4a9edd"),
    ("1 forwarding hop",             "#9a8ae0"),
    ("2 forwarding hops",            "#7c6ff7"),
    ("4 forwarding hops",            "#5a4faa"),
    ("compressing as it goes",       "#3aab76"),
    ("terminal anchor, same",        "#4a9edd"),
]

# Single-row Zane measurements (T13, T14)
SOLO_COLORS = [
    ("payload scan", "#7c6ff7"),
    ("mixed 10:1",   "#7c6ff7"),
]

# Dynamic-region block kinds (T16)
DYN_COLORS = [
    ("boxed payload",   "#7c6ff7"),
    ("backing store",   "#b8a4ff"),
]

# Boxed-member operations (T17)
BOX_COLORS = [
    ("rehost hosted tree, no guests",       "#7c6ff7"),
    ("rehost hosted tree, every node",      "#5a4faa"),
    ("deep-copy value tree",                "#c45a8a"),
    ("construct fresh value tree",          "#3aab76"),
]


def get_color(impl_name):
    """Pick a colour based on the implementation name."""
    lower = impl_name.lower()

    # Checked before the generic "Arena" prefix, which would otherwise swallow it.
    if "one-field init" in lower:
        return "#2a7d57"

    for key, color in HOP_COLORS:
        if key in lower:
            return color
    for key, color in BOX_COLORS:
        if lower.startswith(key):
            return color
    for key, color in DYN_COLORS:
        if lower.startswith(key):
            return color
    for key, color in SOLO_COLORS:
        if lower.startswith(key):
            return color

    if "zane" in lower or "anchor" in lower or "tether" in lower or "guest" in lower:
        for key, color in ZANE_VARIANTS.items():
            if key in lower:
                return color
        return "#7c6ff7"

    if lower.startswith("frontier bump"):
        return "#3aab76"

    for prefix, color in IMPL_COLORS.items():
        if impl_name.startswith(prefix):
            return color

    if lower.startswith("hosted array shards, concurrent"):
        return "#7c6ff7"
    if lower.startswith("hosted array shards, sequential"):
        return "#3aab76"
    if "sequential" in lower:
        return "#c49a2a"
    if "shuffled" in lower:
        return "#e05a3a"
    if "work-stealing" in lower or "concurrent" in lower:
        return "#7c6ff7"

    return "#6b7280"  # fallback grey
