// N entities held inline in one array, scanned 100 times to sum their hit
// points. N is the program's first argument.
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
	auto entities = new Entity[n];
	foreach (i; 1 .. n + 1) entities[i - 1] = Entity(i, 0.5 * i, 0.25 * i, i / 3);
	long total = 0;
	foreach (pass; 1 .. 101)
		foreach (ref e; entities) total += e.hp;
	writeln(n, " ", total);
}
