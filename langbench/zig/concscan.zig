// N entities held inline in one slice, split into four shards, each shard
// summed by its own concurrent task, 100 times over. N is the program's first
// argument.
const std = @import("std");

const Entity = struct {
    id: i64,
    x: f64,
    y: f64,
    hp: i64,
};

fn shardSum(entities: []const Entity) i64 {
    var sum: i64 = 0;
    for (entities) |e| sum += e.hp;
    return sum;
}

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: usize = if (args.len > 1) std.fmt.parseInt(usize, args[1], 10) catch 100000 else 100000;
    const entities = try gpa.alloc(Entity, n);
    defer gpa.free(entities);
    for (entities, 1..) |*e, i| {
        const id: i64 = @intCast(i);
        const f: f64 = @floatFromInt(id);
        e.* = .{ .id = id, .x = 1.1 * f, .y = 2.2 * f, .hp = @mod(id, 100) + 1 };
    }
    var total: i64 = 0;
    for (1..101) |_| {
        var shards: [4]std.Io.Future(i64) = undefined;
        for (&shards, 0..) |*shard, s| {
            shard.* = try init.io.concurrent(shardSum, .{entities[n * s / 4 .. n * (s + 1) / 4]});
        }
        for (&shards) |*shard| total += shard.await(init.io);
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, total });
    try stdout.interface.flush();
}
