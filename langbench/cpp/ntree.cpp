// Trees whose nodes own a growable list of children: a complete four-way
// tree N levels deep, built, counted and torn down 10 times. N is the
// program's first argument.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

struct Node {
	std::vector<std::unique_ptr<Node>> children;
};

static std::unique_ptr<Node> grow(long depth) {
	auto node = std::make_unique<Node>();
	if (depth > 0)
		for (int k = 0; k < 4; k++) node->children.push_back(grow(depth - 1));
	return node;
}

static long count(const Node &node) {
	long total = 1;
	for (const auto &child : node.children) total += count(*child);
	return total;
}

int main(int argc, char **argv) {
	long depth = argc > 1 ? std::atol(argv[1]) : 6;
	long total = 0;
	for (int round = 1; round <= 10; round++) total += count(*grow(depth));
	std::printf("%ld %ld\n", depth, total);
}
