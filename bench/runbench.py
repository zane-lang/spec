#!/usr/bin/env python3
"""
Zane Memory Model Benchmark Runner

Compiles zane_bench.c, runs it, parses the structured output,
and generates benchmark.html with interactive Chart.js visualisations.

Usage:
    python3 bench/runbench.py              # compile, run, generate HTML
    python3 bench/runbench.py --from-file  # skip compile+run, parse existing results file
"""

import subprocess
import sys
import os
import re
import json
import argparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
C_SOURCE   = os.path.join(SCRIPT_DIR, "zane_bench.c")
BINARY     = os.path.join(SCRIPT_DIR, "zane_bench")
RESULTS      = os.path.join(SCRIPT_DIR, "zane_bench_results.txt")
EXPLANATIONS = os.path.join(SCRIPT_DIR, "explanations.txt")
HTML_OUT     = os.path.join(SCRIPT_DIR, "benchmark.html")

# ─────────────────────────────────────────────────────────────
# Test metadata: short name, title, setup, and per-impl facts.
# These track spec/memory.md: hosting is the default and the guest (&) is
# opt-in; a scope owns a fixed-size region and a dynamic region that never
# share a chunk; anchors live in one runtime-global pool of 8-byte cells; and
# a guest is represented internally by a u32 tether — a segmented offset to an
# anchor cell that either terminates at a payload or forwards to another
# anchor. Each hosted payload stores a u32 backpointer to its terminal cell.
#
# The "Details:" note for each test is NOT stored here. It is read from
# explanations.txt — result interpretation authored after looking at a real
# run — so it describes the measured numbers rather than predicting them.
# ─────────────────────────────────────────────────────────────

TEST_META = {
    "Test 1": {
        "short": "T1 — seq alloc+free",
        "title": "Sequential alloc then sequential free",
        "setup": "Hosts created with zm_host: one fixed-region frontier bump plus a backpointer zeroed to 0, so no anchor exists. Release is a no-op — the fixed-size region reclaims nothing per object; its bytes are dead space until the scope drains.",
        "meta": [
            ("Object size", "32B + 4B backpointer"),
            ("Backpointer init", "0 — no anchor at creation"),
            ("Alloc cost", "one fixed-region bump + zero-write"),
            ("Release cost", "no-op — fixed region reclaims only at drain"),
            ("Anchor created", "never — no guests in this test"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 2": {
        "short": "T2 — random-order free",
        "title": "Sequential alloc, then random-order release (only release timed)",
        "setup": "Alloc and shuffle untimed. In the fixed-size region release is a no-op regardless of order — that region has no free list and no coalescing, so order cannot matter.",
        "meta": [
            ("Object size", "40B runtime"),
            ("Backpointer", "0 — no guest taken"),
            ("Timed phase", "release loop only"),
            ("Release path", "no-op — bulk reclaim at scope drain"),
        ],
    },
    "Test 3": {
        "short": "T3 — mixed sizes",
        "title": "Mixed-size alloc and random-order release",
        "setup": "Raw fixed-region blocks of four sizes, released in random order. The fixed-size region is a pure bump: no size classes, no free list, no coalescing.",
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
        "setup": "Five layouts iterated, no alloc or release during the timed loop.",
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
        "setup": "The growth rules of memory.md §3.6, in order: a new list starts at a 128-byte block; on exhaustion it asks for exactly twice its current block size, checks that size's exact-size stack first, then grows in place only if its block is the dynamic frontier allocation and the doubled size still fits inside the 1 MiB chunk, and otherwise relocates into a fresh block or an oversized span.",
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
        "setup": "Terminal resolution path: a tether is a u32 segmented offset (chunk id + in-chunk word offset) naming a cell in the global anchor pool; the cell's first u32 is the payload's segmented offset and its second identifies the cell as a payload anchor rather than a forwarder (tether → cell → payload → field). Both hops resolve through the chunk directory. Anchor pages are their own chunks, never shared with payloads.",
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
        "setup": "Each spawn writes backpointer = 0. Each kill is a no-op release — these are statically sized hosts in the fixed-size region, which reclaims in bulk at drain.",
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
        "setup": "Phases A+B untimed. Phase C timed. These are fixed-region hosts, so Phase B's releases are no-ops and Phase C simply bumps the frontier past the dead space.",
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
        "setup": "Three Zane variants: no guests (every backpointer stays 0), a single root guest, and one guest per node. All use post-order DFS. Node payloads are fixed-region hosts, so their release is a no-op; each node's child list is a 128-byte cache-line-aligned dynamic block that is pushed back onto its exact-size stack.",
        "meta": [
            ("Tree size", "~4,000 nodes, branch 0–6"),
            ("No guests", "backpointer = 0; no anchor allocated"),
            ("Single root guest", "one anchor; 3,999 nodes unanchored"),
            ("One guest per node", "every node mints its own anchor cell"),
            ("Child lists", "128B dynamic blocks returned to the size stack"),
            ("malloc", "free(node) per node, coalescing on each"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 11": {
        "short": "T11 — stress test",
        "title": "Fragmentation stress: hosts + lists, random spawn / push / kill cycles",
        "setup": "Entities are fixed-region hosts whose release is a no-op; list backing stores are dynamic blocks that start at 128B, double, and go back onto the (size, 64) exact-size stacks when a list dies. This is the workload where the dynamic region's reuse path actually runs.",
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
        "setup": "Four read-only shards of one hosted inline array are summed either sequentially or on four worker threads. Each run asserts that the aggregate hp total matches the deterministic baseline.",
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
        "setup": "100k hosts, ~20% guested, then repeated payload-only field scans. Under the default the anchor pool owns its own chunks and the payload stream stays dense; built with -DZM_INTERLEAVE the cells are taken from the fixed region beside the payloads and every scan drags them through cache. Isolates the pervasive-scan cost of cell placement at a realistic guest density.",
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
        "setup": "Aggregates 10 payload scans per 1 tether-resolve pass over the guested subset. Models the real-world weighting the 1:1 micro-tests hide: the global pool should win the aggregate because the pervasive scan dominates, even though its individual resolve is dearer than the interleaved layout's.",
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
        "setup": "Moving into an already-anchored destination keeps the destination cell terminal and turns the source cell into a forwarder pointing at it (memory.md §4.5), so a guest minted before the move gains one hop per move it survived. Each chain is built by repeatedly rehosting into a freshly guested destination; the timed loop resolves the oldest tether, which walks the whole chain. The last two rows separate the hop count from the cache footprint: one compresses as it resolves, so its first pass walks the chain and the rest are terminal, and the other resolves the terminal identity directly over the identical structure — the floor the compressing row is converging on.",
        "meta": [
            ("Chains", "20,000"),
            ("Passes", "8 per timed run"),
            ("Hop cost", "one dependent anchor-cell load per uncompressed hop"),
            ("Chain build", "depth+1 hosts, each guested before the move"),
            ("Path compression", "first pass walks and rewrites the tether; later passes are terminal"),
            ("Footprint floor", "terminal resolve over the same 4-hop structure"),
            ("Asserted", "a compressed tether equals the chain's terminal identity"),
            ("Retirement", "forwarders return to the pool free stack when the source scope drains"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 16": {
        "short": "T16 — dynamic churn",
        "title": "Dynamic-region block churn: exact-size stacks vs a pure frontier",
        "setup": "Repeated create-and-destroy of equal-size dynamic blocks. memory.md §3.2 gives the dynamic region one LIFO stack per (byte size, alignment) pair: an allocation pops that stack first and only bumps the frontier when it is empty. The second row is the same workload with the stacks bypassed, which is what the fixed-size region does — faster per operation, but it never reuses a byte.",
        "meta": [
            ("Sizes", "128 / 256 / 512B, cache-line aligned"),
            ("Blocks", "2,000 per size per round"),
            ("Rounds", "10 — round 1 bumps, rounds 2-10 pop"),
            ("Reuse key", "(byte size, alignment) — never approximate"),
            ("Asserted", "a freed block is handed back for the same size and alignment, and withheld from a different one"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 17": {
        "short": "T17 — boxed members",
        "title": "Boxed members: rehost relocation vs deep value copy",
        "setup": "A recursive tree whose members are boxed (adt.md §4): a fixed-size handle inline, the payload in the dynamic region sized to exactly the node type. Rehosting a reference-type root relocates every boxed descendant into destination-owned storage recursively (memory.md §3.5), retargeting each contained host's anchor on the way; copying a value tree allocates and copies every payload afresh so the two share no storage (§2.3); constructing a fresh one builds each node once in its final owning payload and copies nothing.",
        "meta": [
            ("Tree", "complete binary, depth 12 — 8,191 nodes"),
            ("Hosted node", "24B — value, two handles, and a 4B backpointer"),
            ("Value node", "16B — value and two handles, no identity"),
            ("Boxed payload", "exact node size, node alignment; no size class, no floor"),
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
    ("after path compression",       "#3aab76"),
    ("terminal anchor, same",        "#4a9edd"),
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

    for key, color in HOP_COLORS:
        if key in lower:
            return color
    for key, color in BOX_COLORS:
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


# ─────────────────────────────────────────────────────────────
# Parse benchmark output
# ─────────────────────────────────────────────────────────────

def parse_results(text):
    """Parse the benchmark text output into structured test data.

    Returns a list of dicts:
        [{
            "test_key":  "Test 1",
            "section":   "Test 1 -- Sequential alloc + sequential free  [32 bytes x 100k]",
            "results":   [
                {"impl": "Zane (lazy anchors)", "median_ns": 647232.0, "min_ns": ..., "max_ns": ..., "median_us": 647.232},
                ...
            ]
        }, ...]
    """
    tests = []
    current = None

    section_re = re.compile(r"\|\s+(Test \d+\s+--\s+.+?)\s*\|")
    result_re  = re.compile(
        r"^\s+(.+?)\s{2,}median\s+([\d.]+)\s+ns\s+min\s+([\d.]+)\s+ns\s+max\s+([\d.]+)\s+ns\s+\(\s*([\d.]+)\s+us\)"
    )

    for line in text.splitlines():
        m = section_re.search(line)
        if m:
            section_str = m.group(1).strip()
            # Extract "Test N" key
            key_m = re.match(r"(Test \d+)", section_str)
            test_key = key_m.group(1) if key_m else section_str
            current = {
                "test_key": test_key,
                "section":  section_str,
                "results":  [],
            }
            tests.append(current)
            continue

        m = result_re.match(line)
        if m and current is not None:
            current["results"].append({
                "impl":      m.group(1).strip(),
                "median_ns": float(m.group(2)),
                "min_ns":    float(m.group(3)),
                "max_ns":    float(m.group(4)),
                "median_us": float(m.group(5)),
            })

    return tests


# ─────────────────────────────────────────────────────────────
# Generate HTML
# ─────────────────────────────────────────────────────────────

def parse_explanations(text):
    """Parse explanations.txt into {"Test N": "note"}.

    Format: one block per test, opened by a line "[Test N]", followed by the
    note text. Lines starting with '#' are comments; blank lines separate
    blocks. Each block's lines are joined into a single paragraph.
    """
    out = {}
    key = None
    buf = []
    header_re = re.compile(r"^\[(Test \d+)\]\s*$")
    for line in text.splitlines():
        stripped = line.strip()
        m = header_re.match(stripped)
        if m:
            if key:
                out[key] = " ".join(buf).strip()
            key = m.group(1)
            buf = []
        elif key is not None:
            if stripped.startswith("#") or not stripped:
                continue
            buf.append(stripped)
    if key:
        out[key] = " ".join(buf).strip()
    return out


def load_explanations():
    """Read explanations.txt if present; return {} otherwise."""
    if os.path.exists(EXPLANATIONS):
        with open(EXPLANATIONS) as f:
            return parse_explanations(f.read())
    return {}


def build_test_js(tests, explanations):
    """Build the JS TESTS array from parsed results + metadata + explanations."""
    js_tests = []
    for t in tests:
        key = t["test_key"]
        meta = TEST_META.get(key, {})

        # Sort by median (fastest first)
        results_sorted = sorted(t["results"], key=lambda r: r["median_us"])

        labels = [r["impl"] for r in results_sorted]
        data   = [round(r["median_us"], 2) for r in results_sorted]
        colors = [get_color(r["impl"]) for r in results_sorted]

        js_meta = [{"label": k, "val": v} for k, v in meta.get("meta", [])]

        entry = {
            "t":       meta.get("short", key),
            "title":   meta.get("title", t["section"]),
            "labels":  labels,
            "data":    data,
            "colors":  colors,
            "meta":    js_meta,
            "setup":   meta.get("setup", ""),
            "details": explanations.get(key, ""),
        }
        js_tests.append(entry)

    return json.dumps(js_tests, indent=2)


def generate_html(tests_json):
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Zane Memory Model Benchmark</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.js"></script>
<style>
  :root {{
    --bg:#0d0f12;--surface:#151820;--border:#242830;--text:#e8eaf0;--muted:#6b7280;
    --accent:#7c6ff7;--tab-bg:#1c2028;--tab-act:#242a35;--info-bg:#131820;--divider:#1e242e;--label:#8b92a0;
  }}
  *{{box-sizing:border-box;margin:0;padding:0;}}
  body{{background:var(--bg);color:var(--text);font-family:'JetBrains Mono','Fira Code','Cascadia Code',monospace;font-size:13px;line-height:1.6;padding:40px 32px 60px;max-width:980px;margin:0 auto;}}
  header{{margin-bottom:36px;border-bottom:1px solid var(--border);padding-bottom:24px;}}
  header h1{{font-size:18px;font-weight:600;letter-spacing:-0.02em;margin-bottom:6px;}}
  header p{{font-size:12px;color:var(--muted);}}
  header p span{{color:var(--accent);}}
  .tabs{{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:20px;}}
  .tab{{padding:5px 11px;font-size:11px;font-family:inherit;font-weight:500;border-radius:4px;border:1px solid var(--border);background:var(--tab-bg);color:var(--muted);cursor:pointer;transition:all .12s;white-space:nowrap;letter-spacing:.01em;}}
  .tab.on{{background:var(--tab-act);color:var(--text);border-color:#363d4d;}}
  .tab:hover:not(.on){{border-color:#363d4d;color:var(--text);}}
  .info{{background:var(--info-bg);border:1px solid var(--border);border-radius:6px;padding:16px 18px;margin-bottom:18px;display:grid;grid-template-columns:1fr 1fr;gap:0 32px;}}
  .info-title{{grid-column:1/-1;font-size:13px;font-weight:600;color:var(--text);margin-bottom:12px;letter-spacing:-0.01em;}}
  .info-col{{display:flex;flex-direction:column;gap:5px;}}
  .info-row{{display:flex;gap:10px;font-size:11.5px;}}
  .info-lbl{{color:var(--label);white-space:nowrap;min-width:104px;}}
  .info-val{{color:var(--text);}}
  .info-div{{grid-column:1/-1;border:none;border-top:1px solid var(--divider);margin:10px 0;}}
  .info-details{{grid-column:1/-1;font-size:11.5px;color:var(--muted);line-height:1.65;border-left:2px solid var(--accent);padding-left:10px;}}
  .info-details strong{{color:var(--label);font-weight:600;}}
  .legend{{display:flex;flex-wrap:wrap;gap:14px;margin-bottom:10px;font-size:11px;color:var(--muted);}}
  .litem{{display:flex;align-items:center;gap:5px;}}
  .sw{{width:10px;height:10px;border-radius:2px;flex-shrink:0;}}
  .chart-wrap{{position:relative;width:100%;}}
</style>
</head>
<body>
<header>
  <h1>Zane Memory Model Benchmark</h1>
  <p><span>20 runs per test · median reported · ns precision</span> — compiled with gcc -O2</p>
</header>
<div class="tabs" id="tabs"></div>
<div class="info" id="info"></div>
<div class="legend" id="leg"></div>
<div class="chart-wrap" id="wrap"><canvas id="ch"></canvas></div>
<script>
const TESTS={tests_json};
let chart=null;
function renderInfo(t){{
  const half=Math.ceil(t.meta.length/2);
  const esc=x=>String(x).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
  const rows=col=>col.map(r=>`<div class="info-row"><span class="info-lbl">${{r.label}}</span><span class="info-val">${{r.val}}</span></div>`).join('');
  document.getElementById('info').innerHTML=
    `<div class="info-title">${{t.title}}</div>`+
    `<div class="info-col">${{rows(t.meta.slice(0,half))}}</div>`+
    `<div class="info-col">${{rows(t.meta.slice(half))}}</div>`+
    `<hr class="info-div">`+
    `<div class="info-details"><strong>Setup: </strong>${{t.setup}}${{t.details ? ' <strong>Details: </strong>'+t.details : ''}}</div>`;
}}
function show(idx){{
  const t=TESTS[idx];
  document.getElementById('wrap').style.height=Math.max(140,t.labels.length*54+80)+'px';
  renderInfo(t);
  document.getElementById('leg').innerHTML=t.labels.map((l,i)=>`<span class="litem"><span class="sw" style="background:${{t.colors[i]}}"></span>${{esc(l)}}</span>`).join('');
  if(chart) chart.destroy();
  chart=new Chart(document.getElementById('ch'),{{
    type:'bar',
    data:{{labels:t.labels,datasets:[{{data:t.data,backgroundColor:t.colors,borderRadius:3,barThickness:32}}]}},
    options:{{
      indexAxis:'y',responsive:true,maintainAspectRatio:false,
      plugins:{{legend:{{display:false}},tooltip:{{backgroundColor:'#1c2230',borderColor:'#2e3545',borderWidth:1,titleColor:'#8b92a0',bodyColor:'#e8eaf0',callbacks:{{label:ctx=>' '+ctx.parsed.x.toFixed(2)+' us  (median of 20 runs)'}}}}}},
      scales:{{x:{{title:{{display:true,text:'us — lower is faster',font:{{size:11,family:'inherit'}},color:'#4a5060'}},grid:{{color:'#1a2030'}},ticks:{{font:{{size:11,family:'inherit'}},color:'#6b7280'}}}},
              y:{{ticks:{{font:{{size:11,family:'inherit'}},color:'#8b92a0',autoSkip:false}},grid:{{display:false}}}}}}
    }}
  }});
}}
const el=document.getElementById('tabs');
TESTS.forEach((t,i)=>{{
  const b=document.createElement('button');
  b.className='tab'+(i===0?' on':'');
  b.textContent=t.t;
  b.onclick=()=>{{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('on'));b.classList.add('on');show(i);}};
  el.appendChild(b);
}});
show(0);
</script>
</body>
</html>"""


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Zane benchmark runner + HTML generator")
    parser.add_argument("--from-file", action="store_true",
                        help="Skip compile+run, parse existing results file")
    args = parser.parse_args()

    if args.from_file:
        print(f"Reading existing results from {RESULTS}")
        with open(RESULTS) as f:
            output = f.read()
    else:
        # Compile
        print("Compiling zane_bench.c ...")
        compile_cmd = ["gcc", "-O2", "-Wall", "-Wextra", "-std=c11", "-pthread",
                        "-o", BINARY, C_SOURCE, "-lm"]
        result = subprocess.run(compile_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Compilation failed:")
            print(result.stderr)
            sys.exit(1)
        print("Compiled successfully.")

        # Run
        print("Running benchmark (this may take a minute) ...")
        result = subprocess.run([BINARY], capture_output=True, text=True, timeout=600)
        if result.returncode != 0:
            print("Benchmark failed:")
            print(result.stderr)
            sys.exit(1)
        output = result.stdout
        print("Benchmark completed.")

        # Save raw results
        with open(RESULTS, "w") as f:
            f.write(output)
        print(f"Results saved to {RESULTS}")

    # Parse
    tests = parse_results(output)
    if not tests:
        print("ERROR: No test results found in output.")
        sys.exit(1)

    print(f"Parsed {len(tests)} tests:")
    for t in tests:
        n_results = len(t["results"])
        impls = ", ".join(r["impl"] for r in t["results"])
        print(f"  {t['test_key']}: {n_results} results — {impls}")

    # Generate HTML
    explanations = load_explanations()
    missing = [t["test_key"] for t in tests if t["test_key"] not in explanations]
    if missing:
        print(f"Note: no explanation in {os.path.basename(EXPLANATIONS)} for: {', '.join(missing)}")
    tests_json = build_test_js(tests, explanations)
    html = generate_html(tests_json)

    with open(HTML_OUT, "w") as f:
        f.write(html)
    print(f"\nGenerated {HTML_OUT}")


if __name__ == "__main__":
    main()
