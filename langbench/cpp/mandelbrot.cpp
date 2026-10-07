// The Mandelbrot set on an N by N grid: how many points stay bounded for 50
// iterations. N is the program's first argument.
#include <cstdio>
#include <cstdlib>

static bool bounded(double cr, double ci) {
	double zr = 0.0, zi = 0.0;
	for (int i = 1; i <= 50; i++) {
		double tr = zr * zr - zi * zi + cr;
		zi = 2.0 * zr * zi + ci;
		zr = tr;
		if (4.0 < zr * zr + zi * zi) return false;
	}
	return true;
}

int main(int argc, char **argv) {
	long n = argc > 1 ? std::atol(argv[1]) : 200;
	long inside = 0;
	for (long y = 0; y < n; y++)
		for (long x = 0; x < n; x++)
			if (bounded(2.0 * x / n - 1.5, 2.0 * y / n - 1.0)) inside++;
	std::printf("%ld %ld\n", n, inside);
}
