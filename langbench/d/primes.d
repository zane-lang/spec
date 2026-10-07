// The primes below a fixed bound, found with a sieve, counted and summed.
import std.stdio;

enum bound = 20000;

void main() {
	auto composite = new bool[bound];
	long count = 0, sum = 0;
	foreach (p; 2 .. bound) {
		if (composite[p]) continue;
		count++;
		sum += p;
		for (long m = cast(long) p * p; m < bound; m += p) composite[m] = true;
	}
	writeln(count, " ", sum);
}
