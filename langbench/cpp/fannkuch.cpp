// The Benchmarks Game's fannkuch-redux: every permutation of N items, each
// flipped by reversing its first k + 1 items while the first item k is not 0.
// N is the program's first argument.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>

int main(int argc, char **argv) {
	int n = argc > 1 ? std::atoi(argv[1]) : 7;
	std::vector<int> perm1(n), perm(n), count(n);
	std::iota(perm1.begin(), perm1.end(), 0);
	long checksum = 0;
	int max_flips = 0, r = n, sign = 1;
	for (;;) {
		for (; r != 1; r--) count[r - 1] = r;
		perm = perm1;
		int flips = 0;
		for (int k = perm[0]; k != 0; k = perm[0]) {
			std::reverse(perm.begin(), perm.begin() + k + 1);
			flips++;
		}
		max_flips = std::max(max_flips, flips);
		checksum += sign * flips;
		sign = -sign;
		for (;;) {
			if (r == n) {
				std::printf("%ld %d\n", checksum, max_flips);
				return 0;
			}
			std::rotate(perm1.begin(), perm1.begin() + 1, perm1.begin() + r + 1);
			if (--count[r] > 0) break;
			r++;
		}
	}
}
