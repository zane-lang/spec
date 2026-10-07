/* The Benchmarks Game's fannkuch-redux: every permutation of N items, each
   flipped by reversing its first k + 1 items while the first item k is not 0.
   N is the program's first argument. */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 7;
	int perm1[32], perm[32], count[32];
	for (int i = 0; i < n; i++) perm1[i] = i;
	long checksum = 0;
	int max_flips = 0, r = n, sign = 1;
	for (;;) {
		for (; r != 1; r--) count[r - 1] = r;
		for (int i = 0; i < n; i++) perm[i] = perm1[i];
		int flips = 0;
		for (int k = perm[0]; k != 0; k = perm[0]) {
			for (int i = 0, j = k; i < j; i++, j--) {
				int t = perm[i];
				perm[i] = perm[j];
				perm[j] = t;
			}
			flips++;
		}
		if (max_flips < flips) max_flips = flips;
		checksum += sign * flips;
		sign = -sign;
		for (;;) {
			if (r == n) {
				printf("%ld %d\n", checksum, max_flips);
				return 0;
			}
			int first = perm1[0];
			for (int i = 0; i < r; i++) perm1[i] = perm1[i + 1];
			perm1[r] = first;
			if (--count[r] > 0) break;
			r++;
		}
	}
}
