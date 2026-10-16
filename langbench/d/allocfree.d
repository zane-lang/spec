// N small objects allocated one by one into an array, then released in
// random order, 10 times over. N is the program's first argument.
import std.conv : to, ConvException;
import std.stdio;

struct Item {
	long id;
	double x, y;
	long hp;
}

struct Rng {
	long state = 42;
	long below(long bound) {
		state = (state * 1103515245 + 12345) & 2147483647;
		return state % bound;
	}
}

void main(string[] args) {
	long n = 10000;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	Rng rng;
	long hash = 0;
	foreach (round; 1 .. 11) {
		Item*[] items;
		foreach (i; 1 .. n + 1) items ~= new Item(i, 0.5 * i, 0.25 * i, i / 3);
		foreach (k; 1 .. n + 1) {
			size_t index = rng.below(items.length);
			Item* item = items[index];
			items[index] = items[$ - 1];
			items[$ - 1] = null;
			items = items[0 .. $ - 1];
			hash = (hash * 31 + item.id) & 2147483647;
		}
	}
	writeln(n, " ", hash);
}
