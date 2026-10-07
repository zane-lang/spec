// The Benchmarks Game's binary-trees: perfect binary trees built, walked and
// torn down, many small ones and one that lives through the run.
import std.algorithm : max;
import std.conv : to, ConvException;
import std.stdio;

class Node {
	Node left, right;
	this(Node left, Node right) {
		this.left = left;
		this.right = right;
	}
}

Node make(long depth) {
	if (depth == 0) return new Node(null, null);
	return new Node(make(depth - 1), make(depth - 1));
}

long check(Node n) {
	return n.left is null ? 1 : 1 + check(n.left) + check(n.right);
}

void main(string[] args) {
	long n = 10;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	long maxDepth = max(n, 6);
	long stretch = maxDepth + 1;
	writeln("stretch ", stretch, " ", check(make(stretch)));
	auto longLived = make(maxDepth);
	for (long depth = 4; depth <= maxDepth; depth += 2) {
		long iterations = 1L << (maxDepth - depth + 4);
		long total = 0;
		foreach (i; 0 .. iterations) total += check(make(depth));
		writeln(iterations, " ", depth, " ", total);
	}
	writeln("long ", maxDepth, " ", check(longLived));
}
