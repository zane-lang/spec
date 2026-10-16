// The Benchmarks Game's spectral-norm: the largest singular value of an
// infinite matrix, by 10 rounds of the power method on its N by N corner.
// It prints the value in billionths, truncated. N is the program's first
// argument.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using Vector = std::vector<double>;

static double entry(long i, long j) {
	return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1);
}

static Vector times(const Vector &v) {
	long n = v.size();
	Vector out(n);
	for (long i = 0; i < n; i++) {
		double sum = 0.0;
		for (long j = 0; j < n; j++) sum = sum + entry(i, j) * v[j];
		out[i] = sum;
	}
	return out;
}

static Vector times_transposed(const Vector &v) {
	long n = v.size();
	Vector out(n);
	for (long i = 0; i < n; i++) {
		double sum = 0.0;
		for (long j = 0; j < n; j++) sum = sum + entry(j, i) * v[j];
		out[i] = sum;
	}
	return out;
}

static Vector times_both(const Vector &v) {
	return times_transposed(times(v));
}

int main(int argc, char **argv) {
	long n = argc > 1 ? std::atol(argv[1]) : 100;
	Vector u(n, 1.0), v;
	for (int round = 0; round < 10; round++) {
		v = times_both(u);
		u = times_both(v);
	}
	double vBv = 0.0, vv = 0.0;
	for (long k = 0; k < n; k++) {
		vBv = vBv + u[k] * v[k];
		vv = vv + v[k] * v[k];
	}
	std::printf("%ld %ld\n", n, static_cast<long>(std::sqrt(vBv / vv) * 1e9));
}
