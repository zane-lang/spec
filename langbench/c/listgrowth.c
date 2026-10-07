/* A growable list of entities, filled by appending N items one at a time,
   built and dropped 20 times. N is the program's first argument. */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	long id;
	double x, y;
	long hp;
} entity;

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 100000;
	long total = 0;
	for (long round = 1; round <= 20; round++) {
		long size = 0, capacity = 4;
		entity *list = malloc(capacity * sizeof *list);
		for (long i = 1; i <= n; i++) {
			if (size == capacity) {
				capacity *= 2;
				list = realloc(list, capacity * sizeof *list);
			}
			list[size++] = (entity){i, 0.5 * i, 0.25 * i, i / 3};
		}
		total += list[size - 1].hp + size;
		free(list);
	}
	printf("%ld %ld\n", n, total);
	return 0;
}
