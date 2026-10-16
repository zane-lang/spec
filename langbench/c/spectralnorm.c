/* The Benchmarks Game's spectral-norm: the largest singular value of an
   infinite matrix, by 10 rounds of the power method on its N by N corner.
   It prints the value in billionths, truncated. N is the program's first
   argument. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double entry(long i, long j) {
	return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1);
}

static void times(long n, const double *v, double *out) {
	for (long i = 0; i < n; i++) {
		double sum = 0.0;
		for (long j = 0; j < n; j++) sum = sum + entry(i, j) * v[j];
		out[i] = sum;
	}
}

static void times_transposed(long n, const double *v, double *out) {
	for (long i = 0; i < n; i++) {
		double sum = 0.0;
		for (long j = 0; j < n; j++) sum = sum + entry(j, i) * v[j];
		out[i] = sum;
	}
}

static void times_both(long n, const double *v, double *out, double *tmp) {
	times(n, v, tmp);
	times_transposed(n, tmp, out);
}

int main(int argc, char **argv) {
	long n = argc > 1 ? atol(argv[1]) : 100;
	double *u = malloc(n * sizeof *u), *v = malloc(n * sizeof *v);
	double *tmp = malloc(n * sizeof *tmp);
	for (long i = 0; i < n; i++) u[i] = 1.0;
	for (int round = 0; round < 10; round++) {
		times_both(n, u, v, tmp);
		times_both(n, v, u, tmp);
	}
	double vBv = 0.0, vv = 0.0;
	for (long k = 0; k < n; k++) {
		vBv = vBv + u[k] * v[k];
		vv = vv + v[k] * v[k];
	}
	printf("%ld %ld\n", n, (long)(sqrt(vBv / vv) * 1e9));
	free(u);
	free(v);
	free(tmp);
	return 0;
}
