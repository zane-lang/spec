// The Mandelbrot set on an N by N grid: how many points stay bounded for 50
// iterations. N is the program's first argument.
import std.conv : to, ConvException;
import std.stdio;

bool bounded(double cr, double ci) {
	double zr = 0.0, zi = 0.0;
	foreach (i; 1 .. 51) {
		const tr = zr * zr - zi * zi + cr;
		zi = 2.0 * zr * zi + ci;
		zr = tr;
		if (4.0 < zr * zr + zi * zi) return false;
	}
	return true;
}

void main(string[] args) {
	long n = 200;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	long inside = 0;
	foreach (y; 0 .. n)
		foreach (x; 0 .. n)
			if (bounded(2.0 * x / n - 1.5, 2.0 * y / n - 1.0)) inside++;
	writeln(n, " ", inside);
}
