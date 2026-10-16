/* N entities held inline in one array, split into four shards, each shard
   summed on its own thread, 100 times over. N is the program's first
   argument. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	long id;
	double x, y;
	long hp;
} entity;

typedef struct {
	const entity *entities;
	long first, last, sum;
} shard;

static void *shard_sum(void *arg) {
	shard *s = arg;
	long sum = 0;
	for (long k = s->first; k < s->last; k++) sum += s->entities[k].hp;
	s->sum = sum;
	return NULL;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 100000;
	entity *entities = malloc(n * sizeof *entities);
	for (long i = 1; i <= n; i++)
		entities[i - 1] = (entity){i, 1.1 * i, 2.2 * i, i % 100 + 1};
	long total = 0;
	for (int pass = 1; pass <= 100; pass++) {
		pthread_t threads[4];
		shard shards[4];
		for (int s = 0; s < 4; s++) {
			shards[s] = (shard){entities, n * s / 4, n * (s + 1) / 4, 0};
			pthread_create(&threads[s], NULL, shard_sum, &shards[s]);
		}
		for (int s = 0; s < 4; s++) {
			pthread_join(threads[s], NULL);
			total += shards[s].sum;
		}
	}
	printf("%ld %ld\n", n, total);
	free(entities);
	return 0;
}
