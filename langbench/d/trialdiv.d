// The primes up to N, counted by trial division against a table of small
// primes built with a sieve. N is the program's first argument.
import std.conv : to, ConvException;
import std.stdio;

long[] primesBelow(long bound) {
	auto composite = new bool[bound];
	long[] primes;
	foreach (p; 2 .. bound) {
		if (composite[p]) continue;
		primes ~= p;
		for (long m = p * p; m < bound; m += p) composite[m] = true;
	}
	return primes;
}

bool isPrime(long k, const long[] table) {
	foreach (p; table) {
		if (k < p * p) return true;
		if (k % p == 0) return false;
	}
	return true;
}

void main(string[] args) {
	auto table = primesBelow(4000);
	long n = 100000;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	long count = 0;
	foreach (k; 2 .. n + 1)
		if (isPrime(k, table)) count++;
	writeln(n, " ", count);
}
