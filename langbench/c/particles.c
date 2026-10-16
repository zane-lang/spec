/* A particle system over F frames: each frame spawns a burst of 60
   particles with 10 to 30 frames to live, then moves the living and
   expires the rest. Order does not matter, so an expired particle's slot
   takes the last one. F is the program's first argument. */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	double x, y, vx, vy;
	long ttl, color;
} particle;

static long state = 42;

static long below(long bound) {
	state = (state * 1103515245 + 12345) & 2147483647;
	return state % bound;
}

static particle **particles;
static long size, capacity;

static void push(particle *p) {
	if (size == capacity) {
		capacity = capacity ? capacity * 2 : 4;
		particles = realloc(particles, capacity * sizeof *particles);
	}
	particles[size++] = p;
}

int main(int argc, char **argv) {
	long frames = argc > 1 ? atol(argv[1]) : 500;
	long total = 0;
	for (long frame = 1; frame <= frames; frame++) {
		for (int s = 0; s < 60; s++) {
			particle *p = malloc(sizeof *p);
			p->x = below(800);
			p->y = below(600);
			p->vx = below(11) - 5;
			p->vy = below(11) - 5;
			p->ttl = 10 + below(21);
			p->color = below(8);
			push(p);
		}
		double sum = 0.0;
		for (long k = 0, u = size; u > 0; u--) {
			particle *p = particles[k];
			p->ttl = p->ttl - 1;
			if (p->ttl < 1) {
				free(p);
				particles[k] = particles[--size];
			} else {
				p->x = p->x + p->vx;
				p->y = p->y + p->vy;
				sum = sum + p->x;
				k++;
			}
		}
		total += (long)sum;
	}
	printf("%ld %ld %ld\n", frames, size, total);
	for (long k = 0; k < size; k++) free(particles[k]);
	free(particles);
	return 0;
}
