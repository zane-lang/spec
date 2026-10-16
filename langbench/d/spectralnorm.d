// The Benchmarks Game's spectral-norm: the largest singular value of an
// infinite matrix, by 10 rounds of the power method on its N by N corner.
// It prints the value in billionths, truncated. N is the program's first
// argument.
import std.conv : to, ConvException;
import std.math : sqrt;
import std.stdio;

double entry(long i, long j) {
	return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1);
}

double[] times(const double[] v) {
	auto out_ = new double[v.length];
	foreach (i; 0 .. v.length) {
		double sum = 0.0;
		foreach (j; 0 .. v.length) sum = sum + entry(i, j) * v[j];
		out_[i] = sum;
	}
	return out_;
}

double[] timesTransposed(const double[] v) {
	auto out_ = new double[v.length];
	foreach (i; 0 .. v.length) {
		double sum = 0.0;
		foreach (j; 0 .. v.length) sum = sum + entry(j, i) * v[j];
		out_[i] = sum;
	}
	return out_;
}

double[] timesBoth(const double[] v) {
	return timesTransposed(times(v));
}

void main(string[] args) {
	long n = 100;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	auto u = new double[n];
	u[] = 1.0;
	double[] v;
	foreach (round; 0 .. 10) {
		v = timesBoth(u);
		u = timesBoth(v);
	}
	double vBv = 0.0, vv = 0.0;
	foreach (k; 0 .. n) {
		vBv = vBv + u[k] * v[k];
		vv = vv + v[k] * v[k];
	}
	writeln(n, " ", cast(long)(sqrt(vBv / vv) * 1e9));
}
