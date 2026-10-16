// A game loop over F frames: each frame spawns 30 entities, kills the 20
// oldest, and updates the rest, removing those whose hit points run out.
// The array keeps the entities in the order they spawned. F is the
// program's first argument.
import std.algorithm : min, remove;
import std.conv : to, ConvException;
import std.stdio;

class Entity {
	long id;
	double x, y;
	long hp;
	this(long id, double x, double y, long hp) {
		this.id = id;
		this.x = x;
		this.y = y;
		this.hp = hp;
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
	Entity[] entities;
	long total = 0;
	foreach (frame; 1 .. frames + 1) {
		foreach (s; 0 .. 30) {
			double x = rng.below(1000);
			double y = rng.below(1000);
			long id = rng.below(100);
			long hp = 50 + rng.below(50);
			entities ~= new Entity(id, x, y, hp);
		}
		foreach (d; 0 .. min(20, entities.length)) entities = entities.remove(0);
		size_t k = 0;
		foreach (u; 0 .. entities.length) {
			Entity e = entities[k];
			e.x = e.x + e.id * 0.1;
			e.y = e.y + e.hp * 0.05;
			e.hp = e.hp - 1;
			if (e.hp < 1) {
				entities = entities.remove(k);
			} else {
				total += e.hp;
				k++;
			}
		}
		entities.assumeSafeAppend();
	}
	writeln(frames, " ", entities.length, " ", total);
}
