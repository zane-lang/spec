/* N small objects allocated one by one into an array, then released in
   random order, 10 times over. N is the program's first argument. */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	long id;
	double x, y;
	long hp;
} item;

static long state = 42;

static long below(long bound) {
	state = (state * 1103515245 + 12345) & 2147483647;
	return state % bound;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 10000;
	item **items = malloc(n * sizeof *items);
	long hash = 0;
	for (int round = 1; round <= 10; round++) {
		for (long i = 1; i <= n; i++) {
			item *it = malloc(sizeof *it);
			*it = (item){i, 0.5 * i, 0.25 * i, i / 3};
			items[i - 1] = it;
		}
		for (long size = n; size > 0; size--) {
			long k = below(size);
			item *it = items[k];
			items[k] = items[size - 1];
			hash = (hash * 31 + it->id) & 2147483647;
			free(it);
		}
	}
	printf("%ld %ld\n", n, hash);
	free(items);
	return 0;
}
