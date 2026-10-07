/* N entities held inline in one array, scanned 100 times to sum their hit
   points. N is the program's first argument. */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	long id;
	double x, y;
	long hp;
} entity;

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 100000;
	entity *entities = malloc(n * sizeof *entities);
	for (long i = 1; i <= n; i++)
		entities[i - 1] = (entity){i, 0.5 * i, 0.25 * i, i / 3};
	long total = 0;
	for (long pass = 1; pass <= 100; pass++)
		for (long i = 0; i < n; i++) total += entities[i].hp;
	printf("%ld %ld\n", n, total);
	free(entities);
	return 0;
}
