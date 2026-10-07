// The Benchmarks Game's fannkuch-redux: every permutation of N items, each
// flipped by reversing its first k + 1 items while the first item k is not 0.
// N is the program's first argument.
import std.algorithm : bringToFront, max, reverse;
import std.conv : to, ConvException;
import std.range : iota;
import std.array : array;
import std.stdio;

void main(string[] args) {
	int n = 7;
	if (args.length > 1) {
		try n = args[1].to!int;
		catch (ConvException) {}
	}
	auto perm1 = iota(n).array;
	auto perm = new int[n];
	auto count = new int[n];
	long checksum = 0;
	int maxFlips = 0, r = n, sign = 1;
	for (;;) {
		for (; r != 1; r--) count[r - 1] = r;
		perm[] = perm1[];
		int flips = 0;
		for (int k = perm[0]; k != 0; k = perm[0]) {
			reverse(perm[0 .. k + 1]);
			flips++;
		}
		maxFlips = max(maxFlips, flips);
		checksum += sign * flips;
		sign = -sign;
		for (;;) {
			if (r == n) {
				writeln(checksum, " ", maxFlips);
				return;
			}
			bringToFront(perm1[0 .. 1], perm1[1 .. r + 1]);
			if (--count[r] > 0) break;
			r++;
		}
	}
}
