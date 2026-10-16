// N small objects allocated one by one into a vector, then released in
// random order, 10 times over. N is the program's first argument.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

struct Item {
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
	long n = argc > 1 ? std::atol(argv[1]) : 10000;
	Rng rng;
	long hash = 0;
	for (int round = 1; round <= 10; round++) {
		std::vector<std::unique_ptr<Item>> items;
		for (long i = 1; i <= n; i++)
			items.push_back(std::make_unique<Item>(Item{i, 0.5 * i, 0.25 * i, i / 3}));
		for (long k = 1; k <= n; k++) {
			long index = rng.below(items.size());
			std::unique_ptr<Item> item = std::move(items[index]);
			items[index] = std::move(items.back());
			items.pop_back();
			hash = (hash * 31 + item->id) & 2147483647;
		}
	}
	std::printf("%ld %ld\n", n, hash);
}
