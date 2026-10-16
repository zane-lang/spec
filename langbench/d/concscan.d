// N entities held inline in one array, split into four shards, each shard
// summed on its own thread, 100 times over. N is the program's first
// argument.
import std.conv : to, ConvException;
import std.parallelism : task;
import std.stdio;

struct Entity {
	long id;
	double x, y;
	long hp;
}

long shardSum(const(Entity)[] entities) {
	long sum = 0;
	foreach (ref e; entities) sum += e.hp;
	return sum;
}

void main(string[] args) {
	long n = 100000;
	if (args.length > 1) {
		try n = args[1].to!long;
		catch (ConvException) {}
	}
	auto entities = new Entity[n];
	foreach (i; 1 .. n + 1) entities[i - 1] = Entity(i, 1.1 * i, 2.2 * i, i % 100 + 1);
	long total = 0;
	foreach (pass; 1 .. 101) {
		typeof(task!shardSum(entities))[4] shards;
		foreach (s; 0 .. 4) {
			shards[s] = task!shardSum(entities[n * s / 4 .. n * (s + 1) / 4]);
			shards[s].executeInNewThread();
		}
		foreach (shard; shards) total += shard.yieldForce;
	}
	writeln(n, " ", total);
}
