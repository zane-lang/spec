// The Benchmarks Game's binary-trees: perfect binary trees built, walked and
// torn down, many small ones and one that lives through the run.
#include <cstdio>
#include <cstdlib>
#include <memory>

struct Node {
	std::unique_ptr<Node> left, right;
};

static std::unique_ptr<Node> make(long depth) {
	auto n = std::make_unique<Node>();
	if (depth > 0) {
		n->left = make(depth - 1);
		n->right = make(depth - 1);
	}
	return n;
}

static long check(const Node &n) {
	return n.left ? 1 + check(*n.left) + check(*n.right) : 1;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? std::atol(argv[1]) : 10;
	long max_depth = n < 6 ? 6 : n;
	long stretch = max_depth + 1;
	std::printf("stretch %ld %ld\n", stretch, check(*make(stretch)));
	auto long_lived = make(max_depth);
	for (long depth = 4; depth <= max_depth; depth += 2) {
		long iterations = 1L << (max_depth - depth + 4);
		long total = 0;
		for (long i = 1; i <= iterations; i++) total += check(*make(depth));
		std::printf("%ld %ld %ld\n", iterations, depth, total);
	}
	std::printf("long %ld %ld\n", max_depth, check(*long_lived));
}
