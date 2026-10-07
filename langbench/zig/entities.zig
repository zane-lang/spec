// N entities held inline in one slice, scanned 100 times to sum their hit
// points. N is the program's first argument.
const std = @import("std");

const Entity = struct {
    id: i64,
    x: f64,
    y: f64,
    hp: i64,
};

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: i64 = if (args.len > 1) std.fmt.parseInt(i64, args[1], 10) catch 100000 else 100000;
    const entities = try gpa.alloc(Entity, @intCast(n));
    defer gpa.free(entities);
    for (entities, 1..) |*e, i| {
        const id: i64 = @intCast(i);
        const f: f64 = @floatFromInt(id);
        e.* = .{ .id = id, .x = 0.5 * f, .y = 0.25 * f, .hp = @divTrunc(id, 3) };
    }
    var total: i64 = 0;
    for (1..101) |_| {
        for (entities) |e| total += e.hp;
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, total });
    try stdout.interface.flush();
}
