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
RESULTS    = os.path.join(SCRIPT_DIR, "zane_bench_results.txt")
HTML_OUT   = os.path.join(SCRIPT_DIR, "benchmark.html")

# ─────────────────────────────────────────────────────────────
# Test metadata: descriptions, insights, and per-impl info.
# These match the updated Zane memory model (no 'store' keyword,
# ownership is default, ref is opt-in, leaf-only registration,
# ref objects on heap with stack_index for O(1) unregistration).
# ─────────────────────────────────────────────────────────────

TEST_META = {
    "Test 1": {
        "short": "T1 — seq alloc+free",
        "title": "Sequential alloc then sequential free",
        "setup": "Objects created with zm_alloc_lazy: back-ptr set to 0, no anchor allocated. Free reads back-ptr, sees 0, single zm_free.",
        "insight": "Lazy anchors mean objects with no refs pay zero anchor overhead. Zane is competitive with the Pool and far ahead of malloc.",
        "meta": [
            ("Object size", "40B runtime (32B + 8B back-ptr)"),
            ("Back-ptr init", "0 — no anchor at creation"),
            ("Alloc cost", "one zm_alloc + zero-write"),
            ("Free cost", "read back-ptr → 0 → single zm_free"),
            ("Anchor created", "never — no refs in this test"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 2": {
        "short": "T2 — random-order free",
        "title": "Sequential alloc, then random-order free (only free timed)",
        "setup": "Alloc and shuffle untimed. Back-ptr is 0 so free is one stack push per object.",
        "insight": "Free order never matters — always O(1). No coalescing, no adjacency checks.",
        "meta": [
            ("Object size", "40B runtime"),
            ("Back-ptr", "0 — free is a single zm_free"),
            ("Timed phase", "free loop only"),
            ("Anchor overhead", "none — back-ptr is 0"),
        ],
    },
    "Test 3": {
        "short": "T3 — mixed sizes",
        "title": "Mixed-size alloc and random-order free",
        "setup": "Raw block alloc/free of four sizes in random order.",
        "insight": "Direct size-indexed stack dispatch, no coalescing. Zane's free-stack table handles mixed sizes efficiently.",
        "meta": [
            ("Sizes", "8, 16, 32, 64 bytes — cycled evenly"),
            ("Count", "100,000 total (25k per size)"),
            ("Free order", "Fisher-Yates shuffle"),
            ("Note", "raw block allocs — no back-ptr slot"),
        ],
    },
    "Test 4": {
        "short": "T4 — iteration",
        "title": "Iterating 100k entity objects — five layouts",
        "setup": "Five layouts iterated, no alloc/free during the timed loop.",
        "insight": "A fixed-size inline Array stays the closest current-spec analogue to contiguous owned storage. Chunked layouts remain competitive with contiguous storage and avoid the worst pointer-chase penalty.",
        "meta": [
            ("Object type", "Entity { id: i64, x: f64, y: f64, hp: i32 }"),
            ("Object size", "32 bytes"),
            ("Spec analogue", "Array[100000]<Entity> — fixed-size inline storage"),
            ("CChunked", "64 elements × 32B = 2048B per chunk"),
            ("UList", "8 elements × 32B = 256B per chunk"),
            ("Measured op", "sum all hp fields (read-only scan)"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 5": {
        "short": "T5 — buffer growth",
        "title": "Growing an owned contiguous buffer by appending 100k items",
        "setup": "Dynamic List<T> is deferred on main, so this preserves the workload with a user-space growable buffer built from contiguous owned storage. Zane doubles in-place at the frontier. CChunked and UList never copy — they allocate new chunks.",
        "insight": "Zane wins because ~14 frontier bumps beats thousands of malloc calls.",
        "meta": [
            ("Element type", "Entity { id: i64, x: f64, y: f64, hp: i32 }"),
            ("Spec analogue", "growable buffer layered over Array-like inline storage"),
            ("Zane", "~14 in-place frontier doublings, no malloc"),
            ("CChunked", "1,563 malloc calls, zero copies"),
            ("UList", "12,500 malloc calls, zero copies"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 6": {
        "short": "T6 — ref access",
        "title": "Ref access via anchor+ref_obj vs direct pointer",
        "setup": "Full ref dereference path: ref_anchor (stack) → ref_obj (heap) → anchor → object. Two pointer hops vs one for direct access.",
        "insight": "The full ref path adds measurable overhead from two heap indirections. The runtime liveness guard (0xFFFFFFFF sentinel on ref_anchor) is essentially free — branch predictor learns it immediately.",
        "meta": [
            ("Direct", "one pointer dereference — baseline"),
            ("Anchor only", "heap_base + anchor.heapoffset — simulates post-move owning access"),
            ("Full ref path", "ref_anchor → ref_obj → anchor → object (two hops)"),
            ("Liveness guard", "0xFFFFFFFF on ref_anchor.heapoffset — runtime-only check before deref"),
            ("Leaf-only", "ref registers only in the leaf object's anchor"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 7": {
        "short": "T7 — game loop",
        "title": "Simulated game loop: spawn, kill, and update entities each frame",
        "setup": "Each spawn writes back-ptr = 0. Each kill reads back-ptr (0), single zm_free.",
        "insight": "Update/iteration dominates; allocator cost is a small constant per frame. Lazy anchors mean no overhead for objects without refs.",
        "meta": [
            ("Entity size", "40B (32B + 8B back-ptr)"),
            ("Anchor", "never created — no refs"),
            ("Frame count", "500 frames"),
            ("Spawns/frame", "30 new entities"),
            ("Kills/frame", "20 oldest + hp-drained deaths"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 8": {
        "short": "T8 — particle system",
        "title": "Particle system: burst-spawn short-lifetime objects every frame",
        "setup": "Maximum churn. Every death reads back-ptr (0), single zm_free.",
        "insight": "Lazy anchors mean particle churn costs the same as a plain allocator. No overhead for short-lived objects that never get ref'd.",
        "meta": [
            ("Particle size", "32B (24B + 8B back-ptr)"),
            ("Anchor", "never created — no refs"),
            ("Frame count", "500 frames"),
            ("Spawns/frame", "60 particles"),
            ("Lifetime", "TTL = random 10–30 frames"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 9": {
        "short": "T9 — fragmentation",
        "title": "Checkerboard fragmentation then refill — only refill timed",
        "setup": "Phases A+B untimed. Phase C timed. Free-stacks fully populated after Phase B.",
        "insight": "Refill = O(1) free-stack pops. Zane reclaims fragmented holes instantly.",
        "meta": [
            ("Object size", "40B (32B + 8B back-ptr)"),
            ("Anchor", "never created"),
            ("Phase A (prep)", "alloc 100k objects"),
            ("Phase B (prep)", "free every even-indexed"),
            ("Phase C (timed)", "alloc 50k new objects"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 10": {
        "short": "T10 — tree teardown",
        "title": "Cascade destruction — three Zane ref strategies vs malloc and pool",
        "setup": "Three Zane variants: no refs (lazy stays 0), single parent ref (root only gets anchor + ref), individual refs (every node gets ref_anchor → ref_obj → anchor). All use post-order DFS destruction.",
        "insight": "No refs and single parent ref are nearly identical. Individual refs (1 per node) add measurable cost — the anchor teardown on every node (iterate weak_ref_stack, null ref_anchor, free anchor, free ref_obj, free node) adds up over 4,000 nodes. This is why the spec encourages ref'ing the container, not each child.",
        "meta": [
            ("Tree size", "~4,000 nodes, branch 0–6"),
            ("No refs", "back-ptr = 0, single zm_free per node"),
            ("Single parent ref", "one ref to root only; 3,999 nodes ref-free"),
            ("Individual refs", "every node: ref_anchor → ref_obj → anchor"),
            ("malloc", "free(node) per node, coalescing on each"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 11": {
        "short": "T11 — stress test",
        "title": "Fragmentation stress: objects + owned buffers, random spawn / push / kill cycles",
        "setup": "All alloc/free through zm_alloc_lazy / zm_free_lazy. Back-ptr always 0.",
        "insight": "Iteration and computation dominate in a realistic mixed workload. The deferred dynamic-list surface syntax is approximated here with owned inline buffers.",
        "meta": [
            ("Object size", "40B + 8B back-ptr"),
            ("Owned buffers", "256–512B + 8B back-ptr"),
            ("Anchor", "never created — no refs"),
            ("Cycles", "200 cycles"),
            ("Per cycle", "spawn + create buffers + push + update + kill"),
            ("Runs", "20 — median reported"),
        ],
    },
    "Test 12": {
        "short": "T12 — concurrent scan",
        "title": "Concurrent shard scan over four independent Array[25000]<Entity> workloads",
        "setup": "Four read-only shards of the same owned inline array are summed either sequentially or on four worker threads. Each run asserts that the aggregate hp total matches the deterministic baseline.",
        "insight": "This is the benchmark suite's concurrent variant: independent read-only work that maps cleanly to the updated spec's explicit spawn/implicit parallelism model without introducing nondeterministic shared-state effects.",
        "meta": [
            ("Workers", "4"),
            ("Shard size", "25,000 entities"),
            ("Total layout", "Array[100000]<Entity> split into 4 independent shards"),
            ("Correctness", "aggregate hp sum asserted every run"),
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
    "no refs":           "#7c6ff7",
    "single parent":     "#5a4faa",
    "individual refs":   "#b8a4ff",
    "lazy anchors":      "#7c6ff7",
    "mmap":              "#7c6ff7",
    "in-place":          "#7c6ff7",
    "refill":            "#7c6ff7",
}

def get_color(impl_name):
    """Pick a colour based on the implementation name."""
    # Check Zane variants first
    lower = impl_name.lower()
    if "zane" in lower or "anchor" in lower or "ref path" in lower:
        for key, color in ZANE_VARIANTS.items():
            if key in lower:
                return color
        # specific patterns
        if "anchor only" in lower:
            return "#7c6ff7"
        if "full ref" in lower and "null" in lower:
            return "#9a8ae0"
        if "full ref" in lower:
            return "#b8a4ff"
        return "#7c6ff7"

    # Check general patterns
    for prefix, color in IMPL_COLORS.items():
        if impl_name.startswith(prefix):
            return color

    # Pointer variants
    if "sequential" in lower:
        return "#c49a2a"
    if "shuffled" in lower:
        return "#e05a3a"

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

def build_test_js(tests):
    """Build the JS TESTS array from parsed results + metadata."""
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
            "insight": meta.get("insight", ""),
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
  .info-insight{{grid-column:1/-1;font-size:11.5px;color:var(--muted);line-height:1.65;border-left:2px solid var(--accent);padding-left:10px;}}
  .info-insight strong{{color:var(--label);font-weight:600;}}
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
  const rows=col=>col.map(r=>`<div class="info-row"><span class="info-lbl">${{r.label}}</span><span class="info-val">${{r.val}}</span></div>`).join('');
  document.getElementById('info').innerHTML=
    `<div class="info-title">${{t.title}}</div>`+
    `<div class="info-col">${{rows(t.meta.slice(0,half))}}</div>`+
    `<div class="info-col">${{rows(t.meta.slice(half))}}</div>`+
    `<hr class="info-div">`+
    `<div class="info-insight"><strong>Setup: </strong>${{t.setup}} <strong>Key insight: </strong>${{t.insight}}</div>`;
}}
function show(idx){{
  const t=TESTS[idx];
  document.getElementById('wrap').style.height=Math.max(140,t.labels.length*54+80)+'px';
  renderInfo(t);
  document.getElementById('leg').innerHTML=t.labels.map((l,i)=>`<span class="litem"><span class="sw" style="background:${{t.colors[i]}}"></span>${{l}}</span>`).join('');
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
    tests_json = build_test_js(tests)
    html = generate_html(tests_json)

    with open(HTML_OUT, "w") as f:
        f.write(html)
    print(f"\nGenerated {HTML_OUT}")


if __name__ == "__main__":
    main()
