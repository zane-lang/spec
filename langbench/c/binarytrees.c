/* The Benchmarks Game's binary-trees: perfect binary trees built, walked
   and torn down, many small ones and one that lives through the run. */
#include <stdio.h>
#include <stdlib.h>

typedef struct node {
	struct node *left, *right;
} node;

static node *make(long depth) {
	node *n = malloc(sizeof *n);
	if (depth == 0) {
		n->left = n->right = NULL;
	} else {
		n->left = make(depth - 1);
		n->right = make(depth - 1);
	}
	return n;
}

static long check(const node *n) {
	return n->left ? 1 + check(n->left) + check(n->right) : 1;
}

static void release(node *n) {
	if (n->left) {
		release(n->left);
		release(n->right);
	}
	free(n);
}

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 10;
	long max_depth = n < 6 ? 6 : n;
	long stretch = max_depth + 1;
	node *t = make(stretch);
	printf("stretch %ld %ld\n", stretch, check(t));
	release(t);
	node *long_lived = make(max_depth);
	for (long depth = 4; depth <= max_depth; depth += 2) {
		long iterations = 1L << (max_depth - depth + 4);
		long total = 0;
		for (long i = 1; i <= iterations; i++) {
			node *tree = make(depth);
			total += check(tree);
			release(tree);
		}
		printf("%ld %ld %ld\n", iterations, depth, total);
	}
	printf("long %ld %ld\n", max_depth, check(long_lived));
	release(long_lived);
	return 0;
}
