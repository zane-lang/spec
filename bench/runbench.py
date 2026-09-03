#!/usr/bin/env python3
"""
Zane Memory Model Benchmark Runner

Compiles zane_bench.c, runs it, and renders its JSON output into
benchmark.html (a self-contained page, no external dependencies) and
zane_bench_results.txt (the human-readable table).

The harness emits JSON on stdout; nothing here parses formatted text.

Layout:
    zane_bench.c              the harness
    benchmeta.py              per-test metadata and chart colours
    template.html             the page skeleton
    explanations.txt          result-interpretation notes, one block per test
    zane_bench_results.json   measurements (the pinned artifact)
    zane_bench_results.txt    generated from the JSON, for reading
    benchmark.html            generated

The committed results file is the pinned artifact the notes in
explanations.txt quote, so measuring never replaces it on its own: a plain run
renders the page from what it just measured and leaves the file alone, and
--save is the separate act of pinning.

Usage:
    python3 bench/runbench.py                 # compile, run, render
    python3 bench/runbench.py --save          # ... and pin the run as committed results
    python3 bench/runbench.py --from-file     # render from the committed JSON
    python3 bench/runbench.py --json PATH     # render from an arbitrary results file
"""

import argparse
import json
import os
import re
import subprocess
import sys

import benchmeta

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
C_SOURCE     = os.path.join(SCRIPT_DIR, "zane_bench.c")
BINARY       = os.path.join(SCRIPT_DIR, "zane_bench")
RESULTS_JSON = os.path.join(SCRIPT_DIR, "zane_bench_results.json")
RESULTS_TXT  = os.path.join(SCRIPT_DIR, "zane_bench_results.txt")
EXPLANATIONS = os.path.join(SCRIPT_DIR, "explanations.txt")
TEMPLATE     = os.path.join(SCRIPT_DIR, "template.html")
HTML_OUT     = os.path.join(SCRIPT_DIR, "benchmark.html")

COMPILE_CMD = ["gcc", "-O2", "-Wall", "-Wextra", "-std=c11", "-pthread"]


# ─────────────────────────────────────────────────────────────
# Measurements
# ─────────────────────────────────────────────────────────────

def row_stats(row):
    """Return (median_ns, min_ns, max_ns) for a row, or None if it has no timing.

    A row carries either every per-run sample (what the harness emits) or
    precomputed median/min/max (what the migrated pinned results carry).
    A row marked "eliminated" has no timing at all: its loop is a no-op the
    compiler removes, so the only honest thing to report is that fact.
    """
    if row.get("eliminated"):
        return None
    samples = row.get("samples_ns")
    if samples:
        ordered = sorted(samples)
        n = len(ordered)
        median = (ordered[n // 2] if n % 2
                  else (ordered[n // 2 - 1] + ordered[n // 2]) / 2.0)
        return median, ordered[0], ordered[-1]
    median = row["median_ns"]
    return median, row.get("min_ns", median), row.get("max_ns", median)


def load_results(path):
    """Read a results file and check it carries the schema this script renders."""
    with open(path) as f:
        doc = json.load(f)
    if doc.get("schema") != 1:
        sys.exit(f"ERROR: {path} has schema {doc.get('schema')!r}, expected 1")
    if not doc.get("tests"):
        sys.exit(f"ERROR: {path} contains no tests")
    for test in doc["tests"]:
        if not test.get("rows"):
            sys.exit(f"ERROR: {test.get('id')} has no rows")
    return doc


def compile_and_run():
    """Compile the harness and run it, returning its parsed JSON output."""
    print("Compiling zane_bench.c ...")
    result = subprocess.run(COMPILE_CMD + ["-o", BINARY, C_SOURCE, "-lm"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr)
        sys.exit("Compilation failed.")
    if result.stderr.strip():
        print("Compiler warnings:")
        print(result.stderr)
    print("Compiled successfully.")

    print("Running benchmark (this may take a minute) ...")
    result = subprocess.run([BINARY], capture_output=True, text=True, timeout=1800)
    if result.returncode != 0:
        print(result.stderr)
        sys.exit("Benchmark failed.")
    print("Benchmark completed.")

    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        sys.exit(f"ERROR: harness did not emit valid JSON: {exc}")


# ─────────────────────────────────────────────────────────────
# Text rendering
# ─────────────────────────────────────────────────────────────

BANNER_RULE  = "  +" + "=" * 99 + "+"
SECTION_RULE = "  +" + "-" * 98 + "+"


def render_text(doc):
    """Render the reading copy of the results, one block per test."""
    cfg = doc.get("config", {})
    out = ["", BANNER_RULE,
           "  |  Zane Memory Model Benchmark"
           "                                                                      |",
           "  |  N = %d  .  %d runs each  .  MEDIAN reported (ns, 2 d.p.)"
           "                                  |" % (cfg.get("n", 0), cfg.get("runs", 0)),
           BANNER_RULE]

    for test in doc["tests"]:
        title = f"{test['id']} -- {test['title']}"
        out += ["", SECTION_RULE, "  |  %-96s|" % title, SECTION_RULE]
        for row in test["rows"]:
            stats = row_stats(row)
            if stats is None:
                out.append("    %-34s  no-op -- loop eliminated by the compiler"
                           % row["label"])
                continue
            median, low, high = stats
            out.append("    %-34s  median %12.2f ns  min %12.2f ns  max %12.2f ns  (%9.3f us)"
                       % (row["label"], median, low, high, median / 1000.0))

    return "\n".join(out) + "\n"


# ─────────────────────────────────────────────────────────────
# Explanations
# ─────────────────────────────────────────────────────────────

def parse_explanations(text):
    """Parse explanations.txt into {"Test N": "note"}.

    Format: one block per test, opened by a line "[Test N]", followed by the
    note text. Lines starting with '#' are comments; blank lines separate
    blocks. Each block's lines are joined into a single paragraph.
    """
    out, key, buf = {}, None, []
    header_re = re.compile(r"^\[(Test \d+)\]$")
    for line in text.splitlines():
        stripped = line.strip()
        m = header_re.match(stripped)
        if m:
            if key:
                out[key] = " ".join(buf).strip()
            key, buf = m.group(1), []
        elif key is not None and stripped and not stripped.startswith("#"):
            buf.append(stripped)
    if key:
        out[key] = " ".join(buf).strip()
    return out


def load_explanations():
    """Read explanations.txt if present; return {} otherwise."""
    if not os.path.exists(EXPLANATIONS):
        return {}
    with open(EXPLANATIONS) as f:
        return parse_explanations(f.read())


# ─────────────────────────────────────────────────────────────
# HTML rendering
# ─────────────────────────────────────────────────────────────

def build_tests_json(doc, explanations):
    """Build the JS TESTS array from measurements + metadata + explanations."""
    js_tests = []
    for test in doc["tests"]:
        key  = test["id"]
        meta = benchmeta.TEST_META.get(key, {})

        timed = [(r, row_stats(r)) for r in test["rows"]]
        timed = [(r, s) for r, s in timed if s is not None]
        timed.sort(key=lambda pair: pair[1][0])

        eliminated = [r["label"] for r in test["rows"] if r.get("eliminated")]

        js_tests.append({
            "t":          meta.get("short", key),
            "title":      meta.get("title", test["title"]),
            "labels":     [r["label"] for r, _ in timed],
            "data":       [round(s[0] / 1000.0, 3) for _, s in timed],
            "mins":       [round(s[1] / 1000.0, 3) for _, s in timed],
            "maxs":       [round(s[2] / 1000.0, 3) for _, s in timed],
            "colors":     [benchmeta.get_color(r["label"]) for r, _ in timed],
            "eliminated": eliminated,
            "meta":       [{"label": k, "val": v} for k, v in meta.get("meta", [])],
            "setup":      meta.get("setup", ""),
            "note":       explanations.get(key, ""),
        })

    return json.dumps(js_tests, indent=2)


def render_html(tests_json):
    """Fill the page skeleton with the tests array."""
    with open(TEMPLATE) as f:
        template = f.read()
    if "__TESTS_JSON__" not in template:
        sys.exit(f"ERROR: {TEMPLATE} has no __TESTS_JSON__ placeholder")
    # The array is embedded in an inline <script>. "<" never appears outside a
    # JSON string, so escaping it globally touches only string contents, and it
    # stops a label or title closing the script element.
    return template.replace("__TESTS_JSON__", tests_json.replace("<", "\\u003c"))


# ─────────────────────────────────────────────────────────────
# Reporting
# ─────────────────────────────────────────────────────────────

def report(doc, explanations):
    """Print what was rendered, and warn about anything the page will show badly."""
    print(f"Rendered {len(doc['tests'])} tests:")
    for test in doc["tests"]:
        labels = ", ".join(r["label"] for r in test["rows"])
        print(f"  {test['id']}: {len(test['rows'])} rows — {labels}")

    for test in doc["tests"]:
        if test.get("provenance_note"):
            print(f"  ! {test['id']}: {test['provenance_note']}")

    missing = [t["id"] for t in doc["tests"] if t["id"] not in explanations]
    if missing:
        print(f"Note: no explanation for: {', '.join(missing)}")

    unknown = [t["id"] for t in doc["tests"] if t["id"] not in benchmeta.TEST_META]
    if unknown:
        print(f"Note: no metadata in benchmeta.py for: {', '.join(unknown)}")

    grey = [(t["id"], r["label"]) for t in doc["tests"] for r in t["rows"]
            if not r.get("eliminated") and benchmeta.get_color(r["label"]) == "#6b7280"]
    if grey:
        print("Warning: no colour rule matches these labels (they render grey):")
        for key, label in grey:
            print(f"  {key}: {label}")


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────

def main():
    """Pick a source of measurements, then render the reading copy and the page."""
    parser = argparse.ArgumentParser(description="Zane benchmark runner + page generator")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--from-file", action="store_true",
                        help="Render from the committed results file, without re-running")
    source.add_argument("--json", metavar="PATH",
                        help="Render from an arbitrary results file")
    parser.add_argument("--save", action="store_true",
                        help="Pin this run: overwrite the committed results file")
    args = parser.parse_args()

    if args.json:
        print(f"Reading {args.json}")
        doc = load_results(args.json)
    elif args.from_file:
        print(f"Reading {RESULTS_JSON}")
        doc = load_results(RESULTS_JSON)
    else:
        doc = compile_and_run()
        if args.save:
            with open(RESULTS_JSON, "w") as f:
                json.dump(doc, f, indent=2)
                f.write("\n")
            print(f"Pinned this run to {RESULTS_JSON}")
        else:
            print(f"Measured but not pinned. {os.path.basename(RESULTS_JSON)} is "
                  f"unchanged; pass --save to replace it with this run.")

    with open(RESULTS_TXT, "w") as f:
        f.write(render_text(doc))
    print(f"Wrote {RESULTS_TXT}")

    explanations = load_explanations()
    report(doc, explanations)

    with open(HTML_OUT, "w") as f:
        f.write(render_html(build_tests_json(doc, explanations)))
    print(f"Generated {HTML_OUT}")


if __name__ == "__main__":
    main()
