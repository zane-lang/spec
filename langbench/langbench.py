#!/usr/bin/env python3
"""
Zane language benchmark runner

Builds every test in each language, checks that all of them print the output
the test expects, reads how much of each Zane program an optimized build
computed while compiling, times the programs, and renders langbench.html from
the measurements.

Layout:
    zane.coda, zane-lock.coda   the Zane project: the pinned compiler and core
    bin/<test>/, lib/bench/     the Zane programs and what they share
    c/ cpp/ rust/ go/ d/ zig/   the other languages, one file per program
    tests.py                    per-test metadata and per-language build rules
    template.html               the page skeleton
    explanations.txt            result interpretation, one block per test
    langbench_results.json      the pinned measurements
    langbench.html              generated

The committed results file is the pinned artifact the notes in
explanations.txt quote, so measuring never replaces it on its own: a plain run
renders the page from what it just measured and leaves the file alone, and
--save is the separate act of pinning.

Usage:
    python3 langbench/langbench.py                # build, check, time, render
    python3 langbench/langbench.py --save         # ... and pin the run
    python3 langbench/langbench.py --from-file    # render from the committed JSON
    python3 langbench/langbench.py --json PATH    # render from another results file
    python3 langbench/langbench.py --quick        # small sizes, one run: check the pipeline
    python3 langbench/langbench.py --only primes  # one test
"""

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import stat
import statistics
import subprocess
import sys
import tempfile
import time

import tests

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR    = os.path.join(SCRIPT_DIR, "build")
RESULTS_JSON = os.path.join(SCRIPT_DIR, "langbench_results.json")
EXPLANATIONS = os.path.join(SCRIPT_DIR, "explanations.txt")
TEMPLATE     = os.path.join(SCRIPT_DIR, "template.html")
HTML_OUT     = os.path.join(SCRIPT_DIR, "langbench.html")

SCHEMA = 1


# ─────────────────────────────────────────────────────────────
# Building
# ─────────────────────────────────────────────────────────────

def fill(value, slots):
    """Fill the {name} slots of a build rule's string."""
    for name, text in slots.items():
        value = value.replace("{" + name + "}", text)
    return value


def run_checked(cmd, what, env=None, cwd=None):
    """Run a command that must succeed, returning how long it took in seconds."""
    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=cwd)
    elapsed = time.perf_counter() - start
    if result.returncode != 0:
        sys.exit(f"ERROR: {what} failed:\n$ {' '.join(cmd)}\n{result.stdout}{result.stderr}")
    return elapsed


class Toolchain:
    """One language's compiler, with a cache that lives for one harness run.

    A program's build time is measured with the language's standard library
    and the program's dependencies already built, so it counts the program's
    own compilation and link. Go and Zig build their standard library into a
    cache on first use; it is filled here by building a small program first.
    """

    def __init__(self, lang, scratch):
        self.lang = lang
        self.rule = tests.LANGUAGES[lang]
        self.cache = os.path.join(scratch, f"cache-{lang}")
        os.makedirs(self.cache, exist_ok=True)
        self.env = dict(os.environ)
        for name, value in self.rule.get("env", {}).items():
            self.env[name] = fill(value, {"cache": self.cache})
        if "prime" in self.rule:
            src = os.path.join(scratch, f"prime.{self.rule['ext']}")
            with open(src, "w") as f:
                f.write(self.rule["prime"])
            self.build(src, os.path.join(scratch, f"prime-{lang}"))

    def build(self, src, out):
        """Build *src* into *out*, returning the build time in seconds."""
        tmp = tempfile.mkdtemp(dir=self.cache)
        slots = {"src": src, "out": out, "tmp": tmp, "cache": self.cache}
        cmd = [fill(part, slots) for part in self.rule["build"]]
        try:
            return run_checked(cmd, f"building {os.path.relpath(src, SCRIPT_DIR)}", env=self.env)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


def zane(args, what):
    """Run the zane CLI in the project, returning its stdout."""
    result = subprocess.run(["zane"] + args, capture_output=True, text=True, cwd=SCRIPT_DIR)
    if result.returncode != 0:
        sys.exit(f"ERROR: {what} failed:\n$ zane {' '.join(args)}\n{result.stdout}{result.stderr}")
    return result.stdout


def build_zane(test_id):
    """Build one Zane program optimized, with its dependencies already built.

    zane build builds a program's dependencies into out/deps/ and the program
    into out/host/; removing the program alone makes the next build compile
    it again while reusing the dependencies.
    """
    binary = os.path.join(SCRIPT_DIR, "out", "host", test_id)
    if os.path.exists(binary):
        os.unlink(binary)
    start = time.perf_counter()
    zane(["build", test_id], f"zane build {test_id}")
    return time.perf_counter() - start, binary


def build_all(selected, scratch):
    """Build every program, returning {test id: [row]} with each row's binary."""
    print("Building the Zane programs' dependencies ...")
    zane(["build"], "zane build")

    toolchains = {}
    rows = {}
    for test in selected:
        test_rows = []
        print(f"Building {test['id']} ...")
        seconds, binary = build_zane(test["id"])
        test_rows.append({"lang": "zane", "variant": "plain", "binary": binary,
                          "build_s": round(seconds, 4)})
        for lang in tests.LANGUAGE_ORDER[1:]:
            if lang not in toolchains:
                toolchains[lang] = Toolchain(lang, scratch)
            programs = [("plain", test["id"])] + test["variants"].get(lang, [])
            for variant, stem in programs:
                src = os.path.join(SCRIPT_DIR, lang, f"{stem}.{tests.LANGUAGES[lang]['ext']}")
                binary = os.path.join(BUILD_DIR, f"{lang}-{stem}")
                seconds = toolchains[lang].build(src, binary)
                test_rows.append({"lang": lang, "variant": variant, "binary": binary,
                                  "build_s": round(seconds, 4)})
        rows[test["id"]] = test_rows
    return rows


# ─────────────────────────────────────────────────────────────
# Compile-time evaluation
# ─────────────────────────────────────────────────────────────

def loops(test_id, optimize):
    """How many loops the program's code tree keeps, plain or optimized."""
    args = ["inspect", "cgt", test_id] + (["--optimize"] if optimize else [])
    tree = zane(args, f"zane inspect cgt {test_id}")
    return len(re.findall(r"> repeat:", tree))


def fold(test_id):
    """What an optimized build computed while compiling, read from its code tree.

    Every loop a Zane program runs is a repeat node in its code tree. A build
    that computes a loop while compiling replaces it with the result, so
    comparing the plain and the optimized tree says how much was folded.
    """
    plain, optimized = loops(test_id, False), loops(test_id, True)
    if optimized == 0:
        observed = "folded"
    elif optimized < plain:
        observed = "partly folded"
    else:
        observed = "not folded"
    return {"plain_loops": plain, "optimized_loops": optimized, "observed": observed}


# ─────────────────────────────────────────────────────────────
# Running
# ─────────────────────────────────────────────────────────────

def run_once(binary, args):
    """Run a program once, returning (seconds, stdout)."""
    start = time.perf_counter()
    result = subprocess.run([binary] + args, capture_output=True, text=True)
    elapsed = time.perf_counter() - start
    if result.returncode != 0:
        sys.exit(f"ERROR: {binary} exited {result.returncode}:\n{result.stderr}")
    return elapsed, result.stdout


def measure(test, rows, args, expected, runs, warmup):
    """Check every row's output, then time it: *warmup* runs, then *runs* timed."""
    for row in rows:
        _, out = run_once(row["binary"], args)
        if out != expected:
            sys.exit(f"ERROR: {row['lang']} {row['variant']} printed the wrong output for "
                     f"{test['id']} {' '.join(args)}:\n{out}expected:\n{expected}")
    for row in rows:
        label = f"{tests.LANGUAGES[row['lang']]['name']} {row['variant']}"
        print(f"  timing {label} ...")
        for _ in range(warmup):
            run_once(row["binary"], args)
        row["samples_s"] = [round(run_once(row["binary"], args)[0], 6) for _ in range(runs)]


# ─────────────────────────────────────────────────────────────
# Provenance
# ─────────────────────────────────────────────────────────────

def first_line(cmd):
    """The first line a version command prints, or "unavailable"."""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError:
        return "unavailable"
    text = (result.stdout or result.stderr).strip()
    return text.splitlines()[0].rstrip(":") if text else "unavailable"


def lock_commits():
    """The commits zane-lock.coda pins, by key."""
    commits = {}
    with open(os.path.join(SCRIPT_DIR, "zane-lock.coda")) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 3 and re.fullmatch(r"[0-9a-f]{40}", parts[2]):
                commits[parts[0]] = parts[2]
    return commits


def zane_version():
    """The compiler release the project pins."""
    with open(os.path.join(SCRIPT_DIR, "zane.coda")) as f:
        for line in f:
            if line.startswith("zane-version"):
                return line.split()[1]
    return "unknown"


def cpu_model():
    """The CPU's model name, where the system says it."""
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def provenance():
    """What the run was built with and where it ran."""
    commits = lock_commits()
    return {
        "date": datetime.date.today().isoformat(),
        "machine": {"cpu": cpu_model(), "os": platform.platform()},
        "zane": {"version": zane_version(), "compiler_commit": commits.get("zane"),
                 "core_commit": commits.get("core")},
        "toolchains": {lang: first_line(tests.LANGUAGES[lang]["version"])
                       for lang in tests.LANGUAGE_ORDER},
    }


# ─────────────────────────────────────────────────────────────
# Measuring a run
# ─────────────────────────────────────────────────────────────

def run_bench(selected, quick, runs, warmup):
    """Build, check, read folds and time every selected test."""
    if shutil.which("zane") is None:
        sys.exit("ERROR: the zane CLI is not on PATH")
    os.makedirs(BUILD_DIR, exist_ok=True)
    scratch = tempfile.mkdtemp(prefix="langbench-")
    try:
        built = build_all(selected, scratch)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    doc = {"schema": SCHEMA, **provenance(),
           "config": {"runs": runs, "warmup": warmup, "quick": quick}, "tests": []}
    for test in selected:
        args = test["check_args"] if quick else test["args"]
        expected = test["check_expected"] if quick else test["expected"]
        print(f"Running {test['id']} {' '.join(args)} ...")
        rows = built[test["id"]]
        measure(test, rows, args, expected, runs, warmup)
        for row in rows:
            del row["binary"]
        doc["tests"].append({"id": test["id"], "args": args, "profile": test["profile"],
                             "fold": fold(test["id"]), "rows": rows})
    return doc


# ─────────────────────────────────────────────────────────────
# Results files
# ─────────────────────────────────────────────────────────────

def load_results(path):
    """Read a results file and check it carries the schema this script renders."""
    with open(path) as f:
        doc = json.load(f)
    if doc.get("schema") != SCHEMA:
        sys.exit(f"ERROR: {path} has schema {doc.get('schema')!r}, expected {SCHEMA}")
    if not doc.get("tests"):
        sys.exit(f"ERROR: {path} contains no tests")
    return doc


def pin_results(doc):
    """Replace the committed results file atomically, keeping its file mode."""
    try:
        mode = stat.S_IMODE(os.stat(RESULTS_JSON).st_mode)
    except FileNotFoundError:
        umask = os.umask(0)
        os.umask(umask)
        mode = 0o666 & ~umask
    fd, tmp = tempfile.mkstemp(dir=SCRIPT_DIR, prefix=".langbench_results.", text=True)
    try:
        with os.fdopen(fd, "w") as f:
            json.dump(doc, f, indent=2)
            f.write("\n")
        os.chmod(tmp, mode)
        os.replace(tmp, RESULTS_JSON)
    except BaseException:
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass
        raise


# ─────────────────────────────────────────────────────────────
# Explanations
# ─────────────────────────────────────────────────────────────

def load_explanations():
    """Parse explanations.txt into {test id: note}.

    One block per test, opened by a line "[id]". Lines starting with '#' are
    comments; a block's lines are joined into one paragraph, and a blank line
    starts a new paragraph.
    """
    if not os.path.exists(EXPLANATIONS):
        return {}
    out, key, paragraphs, buf = {}, None, [], []

    def close():
        if buf:
            paragraphs.append(" ".join(buf))
            buf.clear()

    with open(EXPLANATIONS) as f:
        for line in f:
            stripped = line.strip()
            header = re.fullmatch(r"\[([a-z0-9_]+)\]", stripped)
            if header:
                close()
                if key:
                    out[key] = paragraphs
                key, paragraphs = header.group(1), []
            elif stripped.startswith("#"):
                continue
            elif not stripped:
                close()
            elif key is not None:
                buf.append(stripped)
    close()
    if key:
        out[key] = paragraphs
    return out


# ─────────────────────────────────────────────────────────────
# Rendering
# ─────────────────────────────────────────────────────────────

def median(samples):
    return statistics.median(samples)


def page_data(doc, explanations, notes_pinned):
    """Everything the page shows, as one JSON document."""
    by_id = {t["id"]: t for t in tests.TESTS}
    out_tests = []
    for result in (doc or {}).get("tests", []):
        meta = by_id.get(result["id"], {})
        rows = []
        for row in result["rows"]:
            samples = row["samples_s"]
            rows.append({
                "lang": row["lang"],
                "name": tests.LANGUAGES[row["lang"]]["name"],
                "variant": row["variant"],
                "median": median(samples),
                "min": min(samples),
                "max": max(samples),
                "build": row["build_s"],
            })
        out_tests.append({
            "id": result["id"],
            "title": meta.get("title", result["id"]),
            "summary": meta.get("summary", ""),
            "args": result["args"],
            "profile": result["profile"],
            "profileText": tests.PROFILES.get(result["profile"], ""),
            "fold": result["fold"],
            "rows": rows,
            "note": explanations.get(result["id"], []),
        })
    return {
        "pinned": notes_pinned,
        "run": None if doc is None else {
            "date": doc.get("date"), "machine": doc.get("machine"),
            "zane": doc.get("zane"), "toolchains": doc.get("toolchains"),
            "config": doc.get("config"),
        },
        "languages": {lang: {"name": spec["name"], "flags": spec["flags"]}
                      for lang, spec in tests.LANGUAGES.items()},
        "tests": out_tests,
    }


def render_html(data):
    """Fill the page skeleton with the page's data."""
    with open(TEMPLATE) as f:
        template = f.read()
    if "__DATA_JSON__" not in template:
        sys.exit(f"ERROR: {TEMPLATE} has no __DATA_JSON__ placeholder")
    # The data sits in an inline <script>; "<" only appears inside JSON
    # strings, so escaping it everywhere keeps a string from closing the tag.
    text = json.dumps(data, indent=1).replace("<", "\\u003c")
    return template.replace("__DATA_JSON__", text)


def print_summary(doc):
    """Print each test's medians, fastest first."""
    for test in doc["tests"]:
        print(f"\n{test['id']} {' '.join(test['args'])}  "
              f"(profile {test['profile']}, Zane {test['fold']['observed']})")
        rows = sorted(test["rows"], key=lambda r: median(r["samples_s"]))
        for row in rows:
            name = f"{tests.LANGUAGES[row['lang']]['name']} {row['variant']}"
            print(f"  {name:<16} run {median(row['samples_s']):9.4f} s   "
                  f"build {row['build_s']:7.3f} s")


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Zane language benchmark")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--from-file", action="store_true",
                        help="Render from the committed results file, without measuring")
    source.add_argument("--json", metavar="PATH",
                        help="Render from another results file")
    parser.add_argument("--save", action="store_true",
                        help="Pin this run: measure, then replace the committed results file")
    parser.add_argument("--quick", action="store_true",
                        help="Run each test at its small size, once, to check the pipeline")
    parser.add_argument("--only", metavar="TEST", action="append",
                        help="Run only this test (repeatable)")
    parser.add_argument("--runs", type=int, default=5, help="Timed runs per program")
    parser.add_argument("--warmup", type=int, default=1, help="Untimed runs before timing")
    args = parser.parse_args()

    if args.save and (args.json or args.from_file):
        parser.error("--save pins a run; it cannot be combined with --from-file or --json")
    if args.save and (args.quick or args.only):
        parser.error("--save pins a full run; it cannot be combined with --quick or --only")

    if args.json:
        doc = load_results(args.json)
    elif args.from_file:
        doc = load_results(RESULTS_JSON) if os.path.exists(RESULTS_JSON) else None
        if doc is None:
            print(f"No pinned run yet: {os.path.basename(RESULTS_JSON)} does not exist.")
    else:
        known = {t["id"] for t in tests.TESTS}
        for name in args.only or []:
            if name not in known:
                parser.error(f"unknown test {name!r}; tests: {', '.join(sorted(known))}")
        selected = [t for t in tests.TESTS if not args.only or t["id"] in args.only]
        runs, warmup = (1, 0) if args.quick else (args.runs, args.warmup)
        doc = run_bench(selected, args.quick, runs, warmup)
        print_summary(doc)
        if args.save:
            pin_results(doc)
            print(f"\nPinned this run to {RESULTS_JSON}")
        else:
            print(f"\nMeasured but not pinned. {os.path.basename(RESULTS_JSON)} is "
                  f"unchanged; pass --save to replace it with this run.")

    notes_pinned = bool(args.from_file or args.save or
                        (args.json and os.path.abspath(args.json) == RESULTS_JSON))
    with open(HTML_OUT, "w") as f:
        f.write(render_html(page_data(doc, load_explanations(), notes_pinned)))
    print(f"Generated {HTML_OUT}")


if __name__ == "__main__":
    main()
