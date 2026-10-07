/* Trees whose nodes own a growable list of children: a complete four-way
   tree N levels deep, built, counted and torn down 10 times. N is the
   program's first argument. */
#include <stdio.h>
#include <stdlib.h>

typedef struct node {
	struct node **children;
	long size, capacity;
} node;

static void push(node *parent, node *child) {
	if (parent->size == parent->capacity) {
		parent->capacity = parent->capacity ? parent->capacity * 2 : 4;
		parent->children = realloc(parent->children,
		                           parent->capacity * sizeof *parent->children);
	}
	parent->children[parent->size++] = child;
}

static node *grow(long depth) {
	node *n = calloc(1, sizeof *n);
	if (depth > 0)
		for (int k = 0; k < 4; k++) push(n, grow(depth - 1));
	return n;
}

static long count(const node *n) {
	long total = 1;
	for (long k = 0; k < n->size; k++) total += count(n->children[k]);
	return total;
}

static void release(node *n) {
	for (long k = 0; k < n->size; k++) release(n->children[k]);
	free(n->children);
	free(n);
}

int main(int argc, char **argv) {
	long depth = argc > 1 ? atol(argv[1]) : 6;
	long total = 0;
	for (int round = 1; round <= 10; round++) {
		node *tree = grow(depth);
		total += count(tree);
		release(tree);
	}
	printf("%ld %ld\n", depth, total);
	return 0;
}
