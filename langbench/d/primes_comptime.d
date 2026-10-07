// The primes below a fixed bound, counted and summed by a sieve the compiler
// must run while it compiles.
import std.stdio;
import std.typecons : Tuple, tuple;

enum bound = 20000;

Tuple!(long, long) sieve() {
	auto composite = new bool[bound];
	long count = 0, sum = 0;
	foreach (p; 2 .. bound) {
		if (composite[p]) continue;
		count++;
		sum += p;
		for (long m = cast(long) p * p; m < bound; m += p) composite[m] = true;
	}
	return tuple(count, sum);
}

void main() {
	enum result = sieve();
	writeln(result[0], " ", result[1]);
}
