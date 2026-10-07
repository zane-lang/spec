/* A perfect binary tree N levels deep, copied whole 20 times; each copy is
   checked and dropped. N is the program's first argument. */
#include <stdio.h>
#include <stdlib.h>

typedef struct node {
	struct node *left, *right;
	long value;
} node;

static node *make(long depth, long value) {
	node *n = malloc(sizeof *n);
	n->value = value;
	if (depth == 0) {
		n->left = n->right = NULL;
	} else {
		n->left = make(depth - 1, 2 * value);
		n->right = make(depth - 1, 2 * value + 1);
	}
	return n;
}

static node *copy(const node *n) {
	node *c = malloc(sizeof *c);
	c->value = n->value;
	c->left = n->left ? copy(n->left) : NULL;
	c->right = n->right ? copy(n->right) : NULL;
	return c;
}

static long sum(const node *n) {
	return n->left ? n->value + sum(n->left) + sum(n->right) : n->value;
}

static void release(node *n) {
	if (n->left) {
		release(n->left);
		release(n->right);
	}
	free(n);
}

int main(int argc, char **argv) {
	long depth = argc > 1 ? atol(argv[1]) : 12;
	node *original = make(depth, 1);
	long total = 0;
	for (int round = 1; round <= 20; round++) {
		node *c = copy(original);
		total += sum(c);
		release(c);
	}
	printf("%ld %ld %ld\n", depth, total, sum(original));
	release(original);
	return 0;
}
