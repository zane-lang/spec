// A particle system over F frames: each frame spawns a burst of 60
// particles with 10 to 30 frames to live, then moves the living and
// expires the rest. Order does not matter, so an expired particle's slot
// takes the last one. F is the program's first argument.
import std.conv : to, ConvException;
import std.stdio;

class Particle {
	double x, y, vx, vy;
	long ttl, color;
	this(double x, double y, double vx, double vy, long ttl, long color) {
		this.x = x;
		this.y = y;
		this.vx = vx;
		this.vy = vy;
		this.ttl = ttl;
		this.color = color;
	}
}

struct Rng {
	long state = 42;
	long below(long bound) {
		state = (state * 1103515245 + 12345) & 2147483647;
		return state % bound;
	}
}

void main(string[] args) {
	long frames = 500;
	if (args.length > 1) {
		try frames = args[1].to!long;
		catch (ConvException) {}
	}
	Rng rng;
	Particle[] particles;
	long total = 0;
	foreach (frame; 1 .. frames + 1) {
		foreach (s; 0 .. 60) {
			double x = rng.below(800);
			double y = rng.below(600);
			double vx = rng.below(11) - 5;
			double vy = rng.below(11) - 5;
			long ttl = 10 + rng.below(21);
			long color = rng.below(8);
			particles ~= new Particle(x, y, vx, vy, ttl, color);
		}
		double sum = 0.0;
		size_t k = 0;
		foreach (u; 0 .. particles.length) {
			Particle p = particles[k];
			p.ttl = p.ttl - 1;
			if (p.ttl < 1) {
				particles[k] = particles[$ - 1];
				particles[$ - 1] = null;
				particles = particles[0 .. $ - 1];
			} else {
				p.x = p.x + p.vx;
				p.y = p.y + p.vy;
				sum = sum + p.x;
				k++;
			}
		}
		particles.assumeSafeAppend();
		total += cast(long) sum;
	}
	writeln(frames, " ", particles.length, " ", total);
}
