// A perfect binary tree N levels deep, copied whole 20 times; each copy is
// checked and dropped. N is the program's first argument.
#include <cstdio>
#include <cstdlib>
#include <memory>

struct Node {
	long value;
	std::unique_ptr<Node> left, right;

	explicit Node(long value) : value(value) {}
	Node(const Node &other)
		: value(other.value),
		  left(other.left ? std::make_unique<Node>(*other.left) : nullptr),
		  right(other.right ? std::make_unique<Node>(*other.right) : nullptr) {}
};

static std::unique_ptr<Node> make(long depth, long value) {
	auto node = std::make_unique<Node>(value);
	if (depth > 0) {
		node->left = make(depth - 1, 2 * value);
		node->right = make(depth - 1, 2 * value + 1);
	}
	return node;
}

static long sum(const Node &node) {
	return node.left ? node.value + sum(*node.left) + sum(*node.right) : node.value;
}

int main(int argc, char **argv) {
	long depth = argc > 1 ? std::atol(argv[1]) : 12;
	const auto original = make(depth, 1);
	long total = 0;
	for (int round = 1; round <= 20; round++) {
		Node copy = *original;
		total += sum(copy);
	}
	std::printf("%ld %ld %ld\n", depth, total, sum(*original));
}
