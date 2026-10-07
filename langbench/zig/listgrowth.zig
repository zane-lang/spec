// A growable list of entities, filled by appending N items one at a time,
// built and dropped 20 times. N is the program's first argument.
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
    var total: i64 = 0;
    for (1..21) |_| {
        var list: std.ArrayList(Entity) = .empty;
        defer list.deinit(gpa);
        var i: i64 = 1;
        while (i <= n) : (i += 1) {
            const f: f64 = @floatFromInt(i);
            try list.append(gpa, .{ .id = i, .x = 0.5 * f, .y = 0.25 * f, .hp = @divTrunc(i, 3) });
        }
        total += list.items[list.items.len - 1].hp + @as(i64, @intCast(list.items.len));
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, total });
    try stdout.interface.flush();
}
