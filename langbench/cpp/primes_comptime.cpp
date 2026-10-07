// The primes below a fixed bound, counted and summed by a sieve the compiler
// must run while it compiles.
#include <array>
#include <cstdio>
#include <utility>

constexpr long bound = 20000;

consteval std::pair<long, long> sieve() {
	std::array<bool, bound> composite{};
	long count = 0, sum = 0;
	for (long p = 2; p < bound; p++) {
		if (composite[p]) continue;
		count++;
		sum += p;
		for (long m = p * p; m < bound; m += p) composite[m] = true;
	}
	return {count, sum};
}

int main() {
	constexpr auto result = sieve();
	std::printf("%ld %ld\n", result.first, result.second);
}
