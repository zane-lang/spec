"""
Per-test metadata and per-language build rules for langbench.py.

Reader-facing prose about each test lives here, not in code comments; the
interpretation of pinned numbers lives in explanations.txt.
"""

# How each language builds one source file into an executable. "{src}" and
# "{out}" are filled in; "{tmp}" is a scratch directory and "{cache}" the
# language's cache for this run. Clang builds C and C++, so they share Zane's
# LLVM backend; every language builds at its level-2 optimization, with the
# safety checks it keeps by default.
LANGUAGES = {
    "zane": {
        "name": "Zane",
        "version": ["zane", "version"],
        "flags": "zane build (zanec --optimize)",
    },
    "c": {
        "name": "C",
        "ext": "c",
        "version": ["clang", "--version"],
        "build": ["clang", "-O2", "-o", "{out}", "{src}"],
        "flags": "clang -O2",
    },
    "cpp": {
        "name": "C++",
        "ext": "cpp",
        "version": ["clang++", "--version"],
        "build": ["clang++", "-O2", "-std=c++20", "-o", "{out}", "{src}"],
        "flags": "clang++ -O2 -std=c++20",
    },
    "rust": {
        "name": "Rust",
        "ext": "rs",
        "version": ["rustc", "--version"],
        "build": ["rustc", "-C", "opt-level=2", "-o", "{out}", "{src}"],
        "flags": "rustc -C opt-level=2",
    },
    "go": {
        "name": "Go",
        "ext": "go",
        "version": ["go", "version"],
        "build": ["go", "build", "-o", "{out}", "{src}"],
        "env": {"GOCACHE": "{cache}", "GO111MODULE": "off"},
        "prime": 'package main\n\nimport "fmt"\n\nfunc main() { fmt.Println("hello") }\n',
        "flags": "go build",
    },
    "d": {
        "name": "D",
        "ext": "d",
        "version": ["ldc2", "--version"],
        "build": ["ldc2", "-O2", "-of={out}", "-od={tmp}", "{src}"],
        "flags": "ldc2 -O2",
    },
    "zig": {
        "name": "Zig",
        "ext": "zig",
        "version": ["zig", "version"],
        "build": ["zig", "build-exe", "-O", "ReleaseSafe", "-femit-bin={out}",
                  "--cache-dir", "{cache}", "--global-cache-dir", "{cache}", "{src}"],
        "prime": 'const std = @import("std");\n\n'
                 'pub fn main() void {\n    std.debug.print("hello\\n", .{});\n}\n',
        "flags": "zig build-exe -O ReleaseSafe",
    },
}

# The order rows appear in on the page.
LANGUAGE_ORDER = ["zane", "c", "cpp", "rust", "go", "d", "zig"]

# What a fold profile means, shown beside each test.
PROFILES = {
    "static": "Reads no input: an optimized build may compute the whole "
              "program while it compiles.",
    "setup":  "A fixed setup may be computed while compiling; the work its "
              "input drives runs when the program does.",
    "input":  "Reads its size first, so nothing it computes is known while "
              "compiling.",
}

# Each test: the programs, the size the bench runs it at, and the output every
# language must print at that size. "check" is a small size for --quick.
# "variants" lists the extra programs of a language beyond its plain one, as
# (variant, file stem).
TESTS = [
    {
        "id": "primes",
        "title": "Sieve of Eratosthenes, fixed bound",
        "profile": "static",
        "summary": "Counts and sums the primes below 20,000 with a sieve. "
                   "Nothing it computes depends on input, so it measures how "
                   "much of a program each compiler finishes on its own. The "
                   "comptime rows ask the compiler to run the sieve while it "
                   "compiles: constexpr and consteval in C++, a const fn in "
                   "Rust, an enum initializer in D and comptime in Zig. C and "
                   "Go have no way to ask.",
        "args": [],
        "expected": "2262 21171191\n",
        "check_args": [],
        "check_expected": "2262 21171191\n",
        "variants": {
            "cpp": [("comptime", "primes_comptime")],
            "rust": [("comptime", "primes_comptime")],
            "d": [("comptime", "primes_comptime")],
            "zig": [("comptime", "primes_comptime")],
        },
    },
    {
        "id": "trialdiv",
        "title": "Trial division against a prime table",
        "profile": "setup",
        "summary": "Builds a table of the primes below 4,000 with a sieve, "
                   "then counts the primes up to N by trial division against "
                   "it. The table needs no input, so it may be computed while "
                   "compiling; the count depends on N, the program's argument. "
                   "Integer division and remainder dominate.",
        "args": ["10000000"],
        "expected": "10000000 664579\n",
        "check_args": ["100000"],
        "check_expected": "100000 9592\n",
        "variants": {},
    },
    {
        "id": "binarytrees",
        "title": "Binary trees (Benchmarks Game)",
        "profile": "input",
        "summary": "The Benchmarks Game's binary-trees at depth N: builds, "
                   "walks and tears down perfect binary trees, many short-lived "
                   "ones and one that lives through the run. It measures "
                   "allocating and freeing many small owned objects. C, C++, "
                   "Rust and Zig free each tree as it goes; Go and D leave "
                   "theirs to a garbage collector; Zane's reference nodes "
                   "own their children and transfer ownership into each parent.",
        "args": ["16"],
        "expected": None,
        "check_args": ["10"],
        "check_expected": None,
        "variants": {},
    },
    {
        "id": "mandelbrot",
        "title": "Mandelbrot set, counted",
        "profile": "input",
        "summary": "Iterates z = z² + c for every point of an N by N grid and "
                   "counts the points that stay within radius 2 for 50 "
                   "iterations. Floating-point arithmetic in a tight loop "
                   "with an early exit; every language does the same "
                   "operations in the same order, so all count the same "
                   "points. The Benchmarks Game version writes a bitmap; "
                   "this one counts, since Zane has no bit operations.",
        "args": ["3000"],
        "expected": "3000 3572354\n",
        "check_args": ["200"],
        "check_expected": "200 15899\n",
        "variants": {},
    },
    {
        "id": "fannkuch",
        "title": "Fannkuch-redux (Benchmarks Game)",
        "profile": "input",
        "summary": "Visits every permutation of N items and flips each one, "
                   "reversing its first k + 1 items while its first item k "
                   "is not 0, then prints a checksum and the most flips any "
                   "permutation took. Small integer lists, indexed and "
                   "rewritten in place. Zane has no while loop, so its "
                   "version counts the N! permutations with a bounded loop "
                   "and flips by recursion.",
        "args": ["10"],
        "expected": "73196 38\n",
        "check_args": ["7"],
        "check_expected": "228 16\n",
        "variants": {},
    },
    {
        "id": "entities",
        "title": "Scanning entities held inline",
        "profile": "input",
        "summary": "Fills a list with N entities of four 8-byte fields each, "
                   "held inline, then sums one field over all of them 100 "
                   "times. A read-only scan over contiguous storage, the "
                   "inline layout of memorybench's Test 4.",
        "args": ["3000000"],
        "expected": "3000000 149999950000000\n",
        "check_args": ["100000"],
        "check_expected": "100000 166665000000\n",
        "variants": {},
    },
    {
        "id": "listgrowth",
        "title": "Growing a list by appending",
        "profile": "input",
        "summary": "Appends N entities to an empty list one at a time, then "
                   "drops it, 20 times over. It measures how a growable list "
                   "reallocates as it fills, as memorybench's Test 5 does "
                   "for Zane's backing stores. Every language starts from "
                   "an empty list and lets its own growth policy run.",
        "args": ["2000000"],
        "expected": "2000000 53333320\n",
        "check_args": ["100000"],
        "check_expected": "100000 2666660\n",
        "variants": {},
    },
    {
        "id": "ntree",
        "title": "Building and tearing down a tree of lists",
        "profile": "input",
        "summary": "Builds a complete four-way tree N levels deep in which "
                   "every node owns a growable list of its children, counts "
                   "its nodes, and drops it, 10 times over. Teardown walks "
                   "every node and frees every child list, the cascade of "
                   "memorybench's Test 10. Go and D leave the work to their "
                   "garbage collectors.",
        "args": ["10"],
        "expected": "10 13981010\n",
        "check_args": ["6"],
        "check_expected": "6 54610\n",
        "variants": {},
    },
    {
        "id": "treecopy",
        "title": "Deep-copying a tree",
        "profile": "input",
        "summary": "Builds a perfect binary tree N levels deep once, then "
                   "copies it whole 20 times, summing and dropping each "
                   "copy. In Zane the tree is a value with boxed members, so "
                   "an ordinary assignment copies all of it, the deep copy "
                   "of memorybench's Test 14; the other languages call a "
                   "clone written for the purpose or derived by the "
                   "compiler.",
        "args": ["18"],
        "expected": "18 2748773826560 137438691328\n",
        "check_args": ["12"],
        "check_expected": "12 671006720 33550336\n",
        "variants": {},
    },
]


def expected_binarytrees(depth):
    """The output binary-trees prints at *depth*, computed rather than stored."""
    max_depth = max(depth, 6)
    lines = [f"stretch {max_depth + 1} {2 ** (max_depth + 2) - 1}"]
    for d in range(4, max_depth + 1, 2):
        iterations = 2 ** (max_depth - d + 4)
        lines.append(f"{iterations} {d} {iterations * (2 ** (d + 1) - 1)}")
    lines.append(f"long {max_depth} {2 ** (max_depth + 1) - 1}")
    return "\n".join(lines) + "\n"


for _test in TESTS:
    if _test["id"] == "binarytrees":
        _test["expected"] = expected_binarytrees(int(_test["args"][0]))
        _test["check_expected"] = expected_binarytrees(int(_test["check_args"][0]))
