// N entities held inline in one vector, split into four shards, each shard
// summed on its own thread, 100 times over. N is the program's first
// argument.
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

struct Entity {
	long id;
	double x, y;
	long hp;
};

static long shard_sum(const std::vector<Entity> &entities, long first, long last) {
	long sum = 0;
	for (long k = first; k < last; k++) sum += entities[k].hp;
	return sum;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? std::atol(argv[1]) : 100000;
	std::vector<Entity> entities;
	entities.reserve(n);
	for (long i = 1; i <= n; i++) entities.push_back({i, 1.1 * i, 2.2 * i, i % 100 + 1});
	long total = 0;
	for (int pass = 1; pass <= 100; pass++) {
		long sums[4];
		std::vector<std::jthread> threads;
		for (int s = 0; s < 4; s++)
			threads.emplace_back([&, s] { sums[s] = shard_sum(entities, n * s / 4, n * (s + 1) / 4); });
		threads.clear();
		for (long sum : sums) total += sum;
	}
	std::printf("%ld %ld\n", n, total);
}
