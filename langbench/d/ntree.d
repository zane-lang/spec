// Trees whose nodes own a growable list of children: a complete four-way
// tree N levels deep, built, counted and torn down 10 times. N is the
// program's first argument.
import std.conv : to, ConvException;
import std.stdio;

class Node {
	Node[] children;
}

Node grow(long depth) {
	auto node = new Node;
	if (depth > 0)
		foreach (k; 0 .. 4) node.children ~= grow(depth - 1);
	return node;
}

long count(Node node) {
	long total = 1;
	foreach (child; node.children) total += count(child);
	return total;
}

void main(string[] args) {
	long depth = 6;
	if (args.length > 1) {
		try depth = args[1].to!long;
		catch (ConvException) {}
	}
	long total = 0;
	foreach (round; 1 .. 11) total += count(grow(depth));
	writeln(depth, " ", total);
}
