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
                   "theirs to a garbage collector; Zane's trees are values "
                   "owned by their scope.",
        "args": ["16"],
        "expected": None,
        "check_args": ["10"],
        "check_expected": None,
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
