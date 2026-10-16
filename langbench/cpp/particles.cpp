// A particle system over F frames: each frame spawns a burst of 60
// particles with 10 to 30 frames to live, then moves the living and
// expires the rest. Order does not matter, so an expired particle's slot
// takes the last one. F is the program's first argument.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

struct Particle {
	double x, y, vx, vy;
	long ttl, color;
};

struct Rng {
	long state = 42;
	long below(long bound) {
		state = (state * 1103515245 + 12345) & 2147483647;
		return state % bound;
	}
};

int main(int argc, char **argv) {
	long frames = argc > 1 ? std::atol(argv[1]) : 500;
	Rng rng;
	std::vector<std::unique_ptr<Particle>> particles;
	long total = 0;
	for (long frame = 1; frame <= frames; frame++) {
		for (int s = 0; s < 60; s++) {
			double x = rng.below(800);
			double y = rng.below(600);
			double vx = rng.below(11) - 5;
			double vy = rng.below(11) - 5;
			long ttl = 10 + rng.below(21);
			long color = rng.below(8);
			particles.push_back(std::make_unique<Particle>(Particle{x, y, vx, vy, ttl, color}));
		}
		double sum = 0.0;
		size_t k = 0;
		for (size_t u = particles.size(); u > 0; u--) {
			Particle &p = *particles[k];
			p.ttl = p.ttl - 1;
			if (p.ttl < 1) {
				particles[k] = std::move(particles.back());
				particles.pop_back();
			} else {
				p.x = p.x + p.vx;
				p.y = p.y + p.vy;
				sum = sum + p.x;
				k++;
			}
		}
		total += static_cast<long>(sum);
	}
	std::printf("%ld %zu %ld\n", frames, particles.size(), total);
}
