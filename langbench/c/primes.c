/* The primes below a fixed bound, found with a sieve, counted and summed. */
#include <stdbool.h>
#include <stdio.h>

#define BOUND 20000

int main(void) {
	static bool composite[BOUND];
	long count = 0, sum = 0;
	for (long p = 2; p < BOUND; p++) {
		if (composite[p]) continue;
		count++;
		sum += p;
		for (long m = p * p; m < BOUND; m += p) composite[m] = true;
	}
	printf("%ld %ld\n", count, sum);
	return 0;
}
