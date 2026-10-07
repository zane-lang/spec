/* The primes up to N, counted by trial division against a table of small
   primes built with a sieve. N is the program's first argument. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define TABLE_BOUND 4000

static long table[TABLE_BOUND];
static long table_size;

static void primes_below(long bound) {
	static bool composite[TABLE_BOUND];
	for (long p = 2; p < bound; p++) {
		if (composite[p]) continue;
		table[table_size++] = p;
		for (long m = p * p; m < bound; m += p) composite[m] = true;
	}
}

static bool is_prime(long k) {
	for (long i = 0; i < table_size; i++) {
		long p = table[i];
		if (k < p * p) return true;
		if (k % p == 0) return false;
	}
	return true;
}

int main(int argc, char **argv) {
	primes_below(TABLE_BOUND);
	long n = argc > 1 ? atol(argv[1]) : 100000;
	long count = 0;
	for (long k = 2; k <= n; k++)
		if (is_prime(k)) count++;
	printf("%ld %ld\n", n, count);
	return 0;
}
