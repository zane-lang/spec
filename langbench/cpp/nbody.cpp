// The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
// N steps of 0.01 days. It prints the system's energy before and after, in
// billionths, truncated. N is the program's first argument.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

struct Body {
	double x, y, z, vx, vy, vz, mass;
};

constexpr double pi = 3.141592653589793;
constexpr double solar_mass = 4.0 * pi * pi;
constexpr double year = 365.24;

using System = std::array<Body, 5>;

static System start() {
	System bodies{{
		{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, solar_mass},
		{4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
		 0.00166007664274403694 * year, 0.00769901118419740425 * year, -0.0000690460016972063023 * year,
		 0.000954791938424326609 * solar_mass},
		{8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
		 -0.00276742510726862411 * year, 0.00499852801234917238 * year, 0.0000230417297573763929 * year,
		 0.000285885980666130812 * solar_mass},
		{12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
		 0.00296460137564761618 * year, 0.00237847173959480950 * year, -0.0000296589568540237556 * year,
		 0.0000436624404335156298 * solar_mass},
		{15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
		 0.00268067772490389322 * year, 0.00162824170038242295 * year, -0.0000951592254519715870 * year,
		 0.0000515138902046611451 * solar_mass},
	}};
	double px = 0.0, py = 0.0, pz = 0.0;
	for (const Body &b : bodies) {
		px = px + b.vx * b.mass;
		py = py + b.vy * b.mass;
		pz = pz + b.vz * b.mass;
	}
	bodies[0].vx = -px / solar_mass;
	bodies[0].vy = -py / solar_mass;
	bodies[0].vz = -pz / solar_mass;
	return bodies;
}

static void advance(System &bodies, double dt) {
	for (size_t i = 0; i < bodies.size(); i++) {
		Body &bi = bodies[i];
		for (size_t j = i + 1; j < bodies.size(); j++) {
			Body &bj = bodies[j];
			double dx = bi.x - bj.x;
			double dy = bi.y - bj.y;
			double dz = bi.z - bj.z;
			double d2 = dx * dx + dy * dy + dz * dz;
			double mag = dt / (d2 * std::sqrt(d2));
			bi.vx = bi.vx - dx * bj.mass * mag;
			bi.vy = bi.vy - dy * bj.mass * mag;
			bi.vz = bi.vz - dz * bj.mass * mag;
			bj.vx = bj.vx + dx * bi.mass * mag;
			bj.vy = bj.vy + dy * bi.mass * mag;
			bj.vz = bj.vz + dz * bi.mass * mag;
		}
	}
	for (Body &b : bodies) {
		b.x = b.x + dt * b.vx;
		b.y = b.y + dt * b.vy;
		b.z = b.z + dt * b.vz;
	}
}

static double energy(const System &bodies) {
	double e = 0.0;
	for (size_t i = 0; i < bodies.size(); i++) {
		const Body &b = bodies[i];
		e = e + 0.5 * b.mass * (b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
		for (size_t j = i + 1; j < bodies.size(); j++) {
			double dx = b.x - bodies[j].x;
			double dy = b.y - bodies[j].y;
			double dz = b.z - bodies[j].z;
			e = e - b.mass * bodies[j].mass / std::sqrt(dx * dx + dy * dy + dz * dz);
		}
	}
	return e;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? std::atol(argv[1]) : 1000;
	System bodies = start();
	double before = energy(bodies);
	for (long step = 1; step <= n; step++) advance(bodies, 0.01);
	double after = energy(bodies);
	std::printf("%ld %ld %ld\n", n, static_cast<long>(before * 1e9), static_cast<long>(after * 1e9));
}
