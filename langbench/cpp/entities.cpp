// N entities held inline in one vector, scanned 100 times to sum their hit
// points. N is the program's first argument.
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Entity {
	long id;
	double x, y;
	long hp;
};

int main(int argc, char **argv) {
	long n = argc > 1 ? std::atol(argv[1]) : 100000;
	std::vector<Entity> entities;
	entities.reserve(n);
	for (long i = 1; i <= n; i++) entities.push_back({i, 0.5 * i, 0.25 * i, i / 3});
	long total = 0;
	for (int pass = 1; pass <= 100; pass++)
		for (const Entity &e : entities) total += e.hp;
	std::printf("%ld %ld\n", n, total);
}
