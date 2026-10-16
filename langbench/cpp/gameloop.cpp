// A game loop over F frames: each frame spawns 30 entities, kills the 20
// oldest, and updates the rest, removing those whose hit points run out.
// The vector keeps the entities in the order they spawned. F is the
// program's first argument.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

struct Entity {
	long id;
	double x, y;
	long hp;
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
	std::vector<std::unique_ptr<Entity>> entities;
	long total = 0;
	for (long frame = 1; frame <= frames; frame++) {
		for (int s = 0; s < 30; s++) {
			double x = rng.below(1000);
			double y = rng.below(1000);
			long id = rng.below(100);
			long hp = 50 + rng.below(50);
			entities.push_back(std::make_unique<Entity>(Entity{id, x, y, hp}));
		}
		long kills = std::min<long>(20, entities.size());
		for (long d = 0; d < kills; d++) entities.erase(entities.begin());
		size_t k = 0;
		for (size_t u = entities.size(); u > 0; u--) {
			Entity &e = *entities[k];
			e.x = e.x + e.id * 0.1;
			e.y = e.y + e.hp * 0.05;
			e.hp = e.hp - 1;
			if (e.hp < 1) {
				entities.erase(entities.begin() + k);
			} else {
				total += e.hp;
				k++;
			}
		}
	}
	std::printf("%ld %zu %ld\n", frames, entities.size(), total);
}
