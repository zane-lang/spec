// A growable list of entities, filled by appending N items one at a time,
// built and dropped 20 times. N is the program's first argument.
import std.conv : to, ConvException;
import std.stdio;

struct Entity {
	long id;
	double x, y;
	long hp;
}

void main(string[] args) {
	long n = 100000;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	long total = 0;
	foreach (round; 1 .. 21) {
		Entity[] list;
		foreach (i; 1 .. n + 1) list ~= Entity(i, 0.5 * i, 0.25 * i, i / 3);
		total += list[$ - 1].hp + cast(long) list.length;
	}
	writeln(n, " ", total);
}
