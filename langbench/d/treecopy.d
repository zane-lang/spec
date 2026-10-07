// A perfect binary tree N levels deep, copied whole 20 times; each copy is
// checked and dropped. N is the program's first argument.
import std.conv : to, ConvException;
import std.stdio;

class Node {
	long value;
	Node left, right;

	this(long value, Node left, Node right) {
		this.value = value;
		this.left = left;
		this.right = right;
	}

	Node dup() {
		return new Node(value, left is null ? null : left.dup, right is null ? null : right.dup);
	}
}

Node make(long depth, long value) {
	if (depth == 0) return new Node(value, null, null);
	return new Node(value, make(depth - 1, 2 * value), make(depth - 1, 2 * value + 1));
}

long sum(Node node) {
	return node.left is null ? node.value : node.value + sum(node.left) + sum(node.right);
}

void main(string[] args) {
	long depth = 12;
	if (args.length > 1) {
		try depth = args[1].to!long;
		catch (ConvException) {}
	}
	auto original = make(depth, 1);
	long total = 0;
	foreach (round; 1 .. 21) total += sum(original.dup);
	writeln(depth, " ", total, " ", sum(original));
}
