// The primes up to N, counted by trial division against a table of small
// primes built with a sieve. N is the program's first argument.
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::vector<long> primes_below(long bound) {
	std::vector<bool> composite(bound);
	std::vector<long> primes;
	for (long p = 2; p < bound; p++) {
		if (composite[p]) continue;
		primes.push_back(p);
		for (long m = p * p; m < bound; m += p) composite[m] = true;
	}
	return primes;
}

static bool is_prime(long k, const std::vector<long> &table) {
	for (long p : table) {
		if (k < p * p) return true;
		if (k % p == 0) return false;
	}
	return true;
}

int main(int argc, char **argv) {
	const auto table = primes_below(4000);
	long n = argc > 1 ? std::atol(argv[1]) : 100000;
	long count = 0;
	for (long k = 2; k <= n; k++)
		if (is_prime(k, table)) count++;
	std::printf("%ld %ld\n", n, count);
}
