// A growable list of entities, filled by appending N items one at a time,
// built and dropped 20 times. N is the program's first argument.
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
	long total = 0;
	for (int round = 1; round <= 20; round++) {
		std::vector<Entity> list;
		for (long i = 1; i <= n; i++) list.push_back({i, 0.5 * i, 0.25 * i, i / 3});
		total += list.back().hp + static_cast<long>(list.size());
	}
	std::printf("%ld %ld\n", n, total);
}
