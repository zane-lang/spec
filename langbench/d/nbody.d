// The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
// N steps of 0.01 days. It prints the system's energy before and after, in
// billionths, truncated. N is the program's first argument.
import std.conv : to, ConvException;
import std.math : sqrt;
import std.stdio;

struct Body {
	double x, y, z, vx, vy, vz, mass;
}

Body[5] start() {
	double pi = 3.141592653589793;
	double solarMass = 4.0 * pi * pi;
	double year = 365.24;
	Body[5] bodies = [
		Body(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, solarMass),
		Body(4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
			0.00166007664274403694 * year, 0.00769901118419740425 * year, -0.0000690460016972063023 * year,
			0.000954791938424326609 * solarMass),
		Body(8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
			-0.00276742510726862411 * year, 0.00499852801234917238 * year, 0.0000230417297573763929 * year,
			0.000285885980666130812 * solarMass),
		Body(12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
			0.00296460137564761618 * year, 0.00237847173959480950 * year, -0.0000296589568540237556 * year,
			0.0000436624404335156298 * solarMass),
		Body(15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
			0.00268067772490389322 * year, 0.00162824170038242295 * year, -0.0000951592254519715870 * year,
			0.0000515138902046611451 * solarMass),
	];
	double px = 0.0, py = 0.0, pz = 0.0;
	foreach (ref b; bodies) {
		px = px + b.vx * b.mass;
		py = py + b.vy * b.mass;
		pz = pz + b.vz * b.mass;
	}
	bodies[0].vx = -px / solarMass;
	bodies[0].vy = -py / solarMass;
	bodies[0].vz = -pz / solarMass;
	return bodies;
}

void advance(ref Body[5] bodies, double dt) {
	foreach (i; 0 .. bodies.length) {
		Body* bi = &bodies[i];
		foreach (j; i + 1 .. bodies.length) {
			Body* bj = &bodies[j];
			double dx = bi.x - bj.x;
			double dy = bi.y - bj.y;
			double dz = bi.z - bj.z;
			double d2 = dx * dx + dy * dy + dz * dz;
			double mag = dt / (d2 * sqrt(d2));
			bi.vx = bi.vx - dx * bj.mass * mag;
			bi.vy = bi.vy - dy * bj.mass * mag;
			bi.vz = bi.vz - dz * bj.mass * mag;
			bj.vx = bj.vx + dx * bi.mass * mag;
			bj.vy = bj.vy + dy * bi.mass * mag;
			bj.vz = bj.vz + dz * bi.mass * mag;
		}
	}
	foreach (ref b; bodies) {
		b.x = b.x + dt * b.vx;
		b.y = b.y + dt * b.vy;
		b.z = b.z + dt * b.vz;
	}
}

double energy(ref const Body[5] bodies) {
	double e = 0.0;
	foreach (i, ref b; bodies) {
		e = e + 0.5 * b.mass * (b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
		foreach (j; i + 1 .. bodies.length) {
			double dx = b.x - bodies[j].x;
			double dy = b.y - bodies[j].y;
			double dz = b.z - bodies[j].z;
			e = e - b.mass * bodies[j].mass / sqrt(dx * dx + dy * dy + dz * dz);
		}
	}
	return e;
}

void main(string[] args) {
	long n = 1000;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	Body[5] bodies = start();
	double before = energy(bodies);
	foreach (step; 1 .. n + 1) advance(bodies, 0.01);
	double after = energy(bodies);
	writeln(n, " ", cast(long)(before * 1e9), " ", cast(long)(after * 1e9));
}
