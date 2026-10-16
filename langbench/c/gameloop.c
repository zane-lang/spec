/* A game loop over F frames: each frame spawns 30 entities, kills the 20
   oldest, and updates the rest, removing those whose hit points run out.
   The array keeps the entities in the order they spawned. F is the
   program's first argument. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	long id;
	double x, y;
	long hp;
} entity;

static long state = 42;

static long below(long bound) {
	state = (state * 1103515245 + 12345) & 2147483647;
	return state % bound;
}

static entity **entities;
static long size, capacity;

static void push(entity *e) {
	if (size == capacity) {
		capacity = capacity ? capacity * 2 : 4;
		entities = realloc(entities, capacity * sizeof *entities);
	}
	entities[size++] = e;
}

static void remove_at(long k) {
	free(entities[k]);
	memmove(entities + k, entities + k + 1, (size - k - 1) * sizeof *entities);
	size--;
}

int main(int argc, char **argv) {
	long frames = argc > 1 ? atol(argv[1]) : 500;
	long total = 0;
	for (long frame = 1; frame <= frames; frame++) {
		for (int s = 0; s < 30; s++) {
			entity *e = malloc(sizeof *e);
			e->x = below(1000);
			e->y = below(1000);
			e->id = below(100);
			e->hp = 50 + below(50);
			push(e);
		}
		long kills = size < 20 ? size : 20;
		for (long d = 0; d < kills; d++) remove_at(0);
		for (long k = 0, u = size; u > 0; u--) {
			entity *e = entities[k];
			e->x = e->x + e->id * 0.1;
			e->y = e->y + e->hp * 0.05;
			e->hp = e->hp - 1;
			if (e->hp < 1) {
				remove_at(k);
			} else {
				total += e->hp;
				k++;
			}
		}
	}
	printf("%ld %ld %ld\n", frames, size, total);
	for (long k = 0; k < size; k++) free(entities[k]);
	free(entities);
	return 0;
}
