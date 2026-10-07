// The primes below a fixed bound, found with a sieve, counted and summed.
#include <cstdio>
#include <vector>

constexpr long bound = 20000;

int main() {
	std::vector<bool> composite(bound);
	long count = 0, sum = 0;
	for (long p = 2; p < bound; p++) {
		if (composite[p]) continue;
		count++;
		sum += p;
		for (long m = p * p; m < bound; m += p) composite[m] = true;
	}
	std::printf("%ld %ld\n", count, sum);
}
