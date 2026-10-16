/* The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
   N steps of 0.01 days. It prints the system's energy before and after, in
   billionths, truncated. N is the program's first argument. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	double x, y, z, vx, vy, vz, mass;
} body;

#define PI 3.141592653589793
#define SOLAR_MASS (4.0 * PI * PI)
#define YEAR 365.24
#define BODIES 5

static body bodies[BODIES] = {
	{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, SOLAR_MASS},
	{4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
	 0.00166007664274403694 * YEAR, 0.00769901118419740425 * YEAR, -0.0000690460016972063023 * YEAR,
	 0.000954791938424326609 * SOLAR_MASS},
	{8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
	 -0.00276742510726862411 * YEAR, 0.00499852801234917238 * YEAR, 0.0000230417297573763929 * YEAR,
	 0.000285885980666130812 * SOLAR_MASS},
	{12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
	 0.00296460137564761618 * YEAR, 0.00237847173959480950 * YEAR, -0.0000296589568540237556 * YEAR,
	 0.0000436624404335156298 * SOLAR_MASS},
	{15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
	 0.00268067772490389322 * YEAR, 0.00162824170038242295 * YEAR, -0.0000951592254519715870 * YEAR,
	 0.0000515138902046611451 * SOLAR_MASS},
};

static void offset_momentum(void) {
	double px = 0.0, py = 0.0, pz = 0.0;
	for (int i = 0; i < BODIES; i++) {
		px = px + bodies[i].vx * bodies[i].mass;
		py = py + bodies[i].vy * bodies[i].mass;
		pz = pz + bodies[i].vz * bodies[i].mass;
	}
	bodies[0].vx = -px / SOLAR_MASS;
	bodies[0].vy = -py / SOLAR_MASS;
	bodies[0].vz = -pz / SOLAR_MASS;
}

static void advance(double dt) {
	for (int i = 0; i < BODIES; i++) {
		body *bi = &bodies[i];
		for (int j = i + 1; j < BODIES; j++) {
			body *bj = &bodies[j];
			double dx = bi->x - bj->x;
			double dy = bi->y - bj->y;
			double dz = bi->z - bj->z;
			double d2 = dx * dx + dy * dy + dz * dz;
			double mag = dt / (d2 * sqrt(d2));
			bi->vx = bi->vx - dx * bj->mass * mag;
			bi->vy = bi->vy - dy * bj->mass * mag;
			bi->vz = bi->vz - dz * bj->mass * mag;
			bj->vx = bj->vx + dx * bi->mass * mag;
			bj->vy = bj->vy + dy * bi->mass * mag;
			bj->vz = bj->vz + dz * bi->mass * mag;
		}
	}
	for (int k = 0; k < BODIES; k++) {
		bodies[k].x = bodies[k].x + dt * bodies[k].vx;
		bodies[k].y = bodies[k].y + dt * bodies[k].vy;
		bodies[k].z = bodies[k].z + dt * bodies[k].vz;
	}
}

static double energy(void) {
	double e = 0.0;
	for (int i = 0; i < BODIES; i++) {
		const body *b = &bodies[i];
		e = e + 0.5 * b->mass * (b->vx * b->vx + b->vy * b->vy + b->vz * b->vz);
		for (int j = i + 1; j < BODIES; j++) {
			double dx = b->x - bodies[j].x;
			double dy = b->y - bodies[j].y;
			double dz = b->z - bodies[j].z;
			e = e - b->mass * bodies[j].mass / sqrt(dx * dx + dy * dy + dz * dz);
		}
	}
	return e;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 1000;
	offset_momentum();
	double before = energy();
	for (long step = 1; step <= n; step++) advance(0.01);
	double after = energy();
	printf("%ld %ld %ld\n", n, (long)(before * 1e9), (long)(after * 1e9));
	return 0;
}
