#!/usr/bin/env python3
"""
Zane Memory Model Benchmark Runner

Compiles zane_bench.c, runs it, parses the structured output,
and generates benchmark.html — a self-contained interactive page
(pure HTML/CSS bars, no external dependencies).

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
        "setup": "One fixed-region frontier bump plus a 4-byte backpointer zeroed to 0, so no anchor exists. Release is a no-op — the fixed-size region reclaims only when the scope drains.",
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
        "setup": "Alloc and shuffle untimed. The fixed-size region has no free list and no coalescing, so release is a no-op regardless of order.",
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
            ("Path compression", "first pass walks and rewrites the tether; later passes are terminal"),
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
        data   = [round(r["median_us"], 3) for r in results_sorted]
        mins   = [round(r["min_ns"] / 1000.0, 3) for r in results_sorted]
        maxs   = [round(r["max_ns"] / 1000.0, 3) for r in results_sorted]
        colors = [get_color(r["impl"]) for r in results_sorted]

        js_meta = [{"label": k, "val": v} for k, v in meta.get("meta", [])]

        entry = {
            "t":       meta.get("short", key),
            "title":   meta.get("title", t["section"]),
            "labels":  labels,
            "data":    data,
            "mins":    mins,
            "maxs":    maxs,
            "colors":  colors,
            "meta":    js_meta,
            "setup":   meta.get("setup", ""),
            "details": explanations.get(key, ""),
        }
        js_tests.append(entry)

    return json.dumps(js_tests, indent=2)


HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Zane Memory Model Benchmark</title>
<style>
  :root {
    --bg:#0d0f12;--surface:#151820;--border:#242830;--text:#e8eaf0;--muted:#6b7280;
    --accent:#7c6ff7;--tab-bg:#1c2028;--tab-act:#242a35;--info-bg:#131820;--divider:#1e242e;--label:#8b92a0;
    --track:#151a22;--whisker:#e8eaf0;--prose:#ccd1dc;
  }
  *{box-sizing:border-box;margin:0;padding:0;}
  body{background:var(--bg);color:var(--text);font-family:'JetBrains Mono','Fira Code','Cascadia Code',monospace;font-size:13px;line-height:1.6;padding:40px 32px 60px;max-width:980px;margin:0 auto;}
  header{margin-bottom:36px;border-bottom:1px solid var(--border);padding-bottom:24px;}
  header h1{font-size:18px;font-weight:600;letter-spacing:-0.02em;margin-bottom:6px;}
  header p{font-size:12px;color:var(--muted);}
  header p span{color:var(--accent);}
  .tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:20px;}
  .tab{padding:5px 11px;font-size:11px;font-family:inherit;font-weight:500;border-radius:4px;border:1px solid var(--border);background:var(--tab-bg);color:var(--muted);cursor:pointer;transition:all .12s;white-space:nowrap;letter-spacing:.01em;}
  .tab.on{background:var(--tab-act);color:var(--text);border-color:#363d4d;}
  .tab:hover:not(.on){border-color:#363d4d;color:var(--text);}
  .info{background:var(--info-bg);border:1px solid var(--border);border-radius:6px;padding:16px 18px;margin-bottom:18px;display:grid;grid-template-columns:1fr 1fr;gap:0 32px;}
  .info-title{grid-column:1/-1;font-size:13px;font-weight:600;color:var(--text);margin-bottom:12px;letter-spacing:-0.01em;}
  .info-col{display:flex;flex-direction:column;gap:5px;}
  .info-row{display:flex;gap:10px;font-size:11.5px;}
  .info-lbl{color:var(--label);white-space:nowrap;min-width:104px;flex-shrink:0;}
  .info-val{color:var(--text);}
  .info-div{grid-column:1/-1;border:none;border-top:1px solid var(--divider);margin:10px 0;}
  .info-details{grid-column:1/-1;font-size:11.5px;color:var(--muted);line-height:1.65;border-left:2px solid var(--accent);padding-left:10px;}
  .info-details strong{color:var(--label);font-weight:600;}

  .notes{margin-top:30px;padding-top:20px;border-top:1px solid var(--divider);}
  .notes h2{font-size:10.5px;font-weight:600;text-transform:uppercase;letter-spacing:.09em;color:var(--label);margin-bottom:12px;}
  .notes p{font-size:12.5px;line-height:1.85;color:var(--prose);max-width:74ch;}

  .chart-head{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;gap:12px;flex-wrap:wrap;}
  .chart-cap{font-size:11px;color:var(--muted);}
  .scale-toggle{display:flex;border:1px solid var(--border);border-radius:4px;overflow:hidden;}
  .scale-btn{padding:3px 10px;font-size:10.5px;font-family:inherit;border:none;background:var(--tab-bg);color:var(--muted);cursor:pointer;transition:all .12s;}
  .scale-btn.on{background:var(--tab-act);color:var(--text);}

  .chart{display:flex;flex-direction:column;gap:8px;}
  .row{display:grid;grid-template-columns:230px 1fr 150px;gap:12px;align-items:center;}
  .rname{font-size:11.5px;color:var(--label);text-align:right;line-height:1.35;overflow-wrap:break-word;}
  .row.fastest .rname{color:var(--text);}
  .rtrack{position:relative;height:26px;background:var(--track);border-radius:4px;}
  .rbar{position:absolute;left:0;top:0;bottom:0;border-radius:4px 3px 3px 4px;min-width:2px;transition:width .35s cubic-bezier(.2,.8,.2,1);}
  .rwhisker{position:absolute;top:50%;transform:translateY(-50%);height:1px;background:var(--whisker);opacity:.4;}
  .rwhisker.clipped{background:repeating-linear-gradient(to right,var(--whisker) 0 4px,transparent 4px 7px);}
  .rcap{position:absolute;top:50%;transform:translateY(-50%);width:1px;height:9px;background:var(--whisker);opacity:.4;}
  .rmeta{display:flex;align-items:baseline;gap:8px;white-space:nowrap;}
  .rval{font-size:11.5px;color:var(--text);}
  .rmult{font-size:10.5px;color:var(--muted);}
  .row.fastest .rmult{color:#3aab76;}
  footer{margin-top:28px;font-size:10.5px;color:var(--muted);}

  @media (max-width:720px){
    .row{grid-template-columns:1fr;gap:3px;}
    .rname{text-align:left;}
  }
</style>
</head>
<body>
<header>
  <h1>Zane Memory Model Benchmark</h1>
  <p><span>20 runs per test &middot; median reported &middot; ns precision</span> &mdash; compiled with gcc -O2</p>
</header>
<div class="tabs" id="tabs"></div>
<div class="info" id="info"></div>
<div class="chart-head">
  <span class="chart-cap">bar = median &middot; whisker = min&ndash;max across runs &middot; lower is faster</span>
  <div class="scale-toggle" id="scale">
    <button class="scale-btn" data-s="linear">linear</button>
    <button class="scale-btn" data-s="log">log</button>
  </div>
</div>
<div class="chart" id="chart"></div>
<section class="notes" id="notes"></section>
<footer>Rows sorted fastest &rarr; slowest. The &times; figure is each row's slowdown relative to the fastest row in this test. The axis stops at twice the slowest median, so one outlier run cannot squash every bar; a whisker running past it is drawn dashed and uncapped. Hover a row for exact min / median / max.</footer>
<script>
const TESTS=__TESTS_JSON__;
let cur=0, scale='linear';
const esc=x=>String(x).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');

function fmt(us){
  if(us>=1000) return (us/1000).toFixed(2)+' ms';
  if(us>=1)    return us.toFixed(2)+' \u00b5s';
  return (us*1000).toFixed(1)+' ns';
}

function renderInfo(t){
  const half=Math.ceil(t.meta.length/2);
  const rows=col=>col.map(r=>`<div class="info-row"><span class="info-lbl">${r.label}</span><span class="info-val">${r.val}</span></div>`).join('');
  document.getElementById('info').innerHTML=
    `<div class="info-title">${t.title}</div>`+
    `<div class="info-col">${rows(t.meta.slice(0,half))}</div>`+
    `<div class="info-col">${rows(t.meta.slice(half))}</div>`+
    `<hr class="info-div">`+
    `<div class="info-details"><strong>Setup: </strong>${t.setup}</div>`;
  const n=document.getElementById('notes');
  n.innerHTML=t.details ? `<h2>Reading the result</h2><p>${t.details}</p>` : '';
  n.hidden=!t.details;
}

// The axis follows the whiskers only while they stay within AXIS_CAP times the
// slowest median. Past that a single outlier run would set the scale for every
// row and squash the medians -- the figure the bars exist to compare -- into a
// fraction of the track, so the axis stops at the cap and the whiskers that run
// past it are clipped (drawn dashed and uncapped) instead.
const AXIS_CAP=2;

function axisTop(t){
  const topMedian=Math.max(...t.data);
  return Math.min(Math.max(...t.maxs, ...t.data), topMedian*AXIS_CAP);
}

function widthPct(v,hi,t,s){
  if(v<=0||hi<=0) return 0;
  if(s==='linear') return Math.min(100, v/hi*100);
  const pos=[...t.data,...t.mins].filter(x=>x>0);
  const lo=Math.min(...pos);
  const l0=Math.log(lo/1.6), l1=Math.log(hi*1.02);
  return Math.min(100, Math.max(0,(Math.log(v)-l0)/(l1-l0))*100);
}

function renderChart(t,s){
  const best=Math.min(...t.data);
  const hi=axisTop(t);
  document.getElementById('chart').innerHTML=t.labels.map((label,i)=>{
    const v=t.data[i], mn=t.mins[i], mx=t.maxs[i];
    const w=widthPct(v,hi,t,s), wl=widthPct(mn,hi,t,s), wr=widthPct(mx,hi,t,s);
    const clipped=mx>hi;
    const mult=best>0 ? v/best : 1;
    const multTxt=(mult<1.005) ? 'fastest' : '\u00d7'+(mult>=100?mult.toFixed(0):mult.toFixed(mult>=10?1:2));
    const whisker=(wr-wl>0.4)
      ? `<span class="rwhisker${clipped?' clipped':''}" style="left:${wl}%;width:${wr-wl}%"></span>`+
        `<span class="rcap" style="left:${wl}%"></span>`+
        (clipped ? '' : `<span class="rcap" style="left:${wr}%"></span>`)
      : '';
    return `<div class="row${mult<1.005?' fastest':''}" title="min ${fmt(mn)}  \u00b7  median ${fmt(v)}  \u00b7  max ${fmt(mx)}${clipped?'  (clipped)':''}">`+
      `<span class="rname">${esc(label)}</span>`+
      `<span class="rtrack"><span class="rbar" style="width:${w}%;background:${t.colors[i]}"></span>${whisker}</span>`+
      `<span class="rmeta"><span class="rval">${fmt(v)}</span><span class="rmult">${multTxt}</span></span>`+
      `</div>`;
  }).join('');
  document.querySelectorAll('.scale-btn').forEach(b=>b.classList.toggle('on',b.dataset.s===s));
}

function show(idx){
  cur=idx;
  const t=TESTS[idx];
  renderInfo(t);
  renderChart(t,scale);
}

const el=document.getElementById('tabs');
TESTS.forEach((t,i)=>{
  const b=document.createElement('button');
  b.className='tab'+(i===0?' on':'');
  b.textContent=t.t;
  b.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('on'));b.classList.add('on');show(i);};
  el.appendChild(b);
});
document.querySelectorAll('.scale-btn').forEach(b=>{
  b.onclick=()=>{scale=b.dataset.s;renderChart(TESTS[cur],scale);};
});
show(0);
</script>
</body>
</html>"""


def generate_html(tests_json):
    """Render the full benchmark page, embedding the tests array as JS.

    The page is fully self-contained: bars are plain HTML/CSS (no CDN
    dependency, so it renders offline), and each row carries its own label,
    value, min-max whisker, and slowdown multiplier vs the fastest row --
    no separate legend duplicating the row labels.
    """
    return HTML_TEMPLATE.replace("__TESTS_JSON__", tests_json)


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────

def main():
    """Compile and run the benchmark (or reuse the results file), then emit HTML."""
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
    greyed = [(t["test_key"], r["impl"]) for t in tests for r in t["results"]
              if get_color(r["impl"]) == "#6b7280"]
    if greyed:
        print("Warning: no colour rule matches these labels (they render grey):")
        for key, impl in greyed:
            print(f"  {key}: {impl}")

    tests_json = build_test_js(tests, explanations)
    html = generate_html(tests_json)

    with open(HTML_OUT, "w") as f:
        f.write(html)
    print(f"\nGenerated {HTML_OUT}")


if __name__ == "__main__":
    main()
