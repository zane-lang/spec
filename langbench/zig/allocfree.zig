// N small objects allocated one by one into a list, then released in
// random order, 10 times over. N is the program's first argument.
const std = @import("std");

const Item = struct {
    id: i64,
    x: f64,
    y: f64,
    hp: i64,
};

const Rng = struct {
    state: i64 = 42,

    fn below(self: *Rng, bound: i64) i64 {
        self.state = (self.state * 1103515245 + 12345) & 2147483647;
        return @mod(self.state, bound);
    }
};

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: i64 = if (args.len > 1) std.fmt.parseInt(i64, args[1], 10) catch 10000 else 10000;
    var rng: Rng = .{};
    var hash: i64 = 0;
    for (1..11) |_| {
        var items: std.ArrayList(*Item) = .empty;
        defer items.deinit(gpa);
        var i: i64 = 1;
        while (i <= n) : (i += 1) {
            const item = try gpa.create(Item);
            const f: f64 = @floatFromInt(i);
            item.* = .{ .id = i, .x = 0.5 * f, .y = 0.25 * f, .hp = @divTrunc(i, 3) };
            try items.append(gpa, item);
        }
        for (0..@intCast(n)) |_| {
            const index: usize = @intCast(rng.below(@intCast(items.items.len)));
            const item = items.swapRemove(index);
            hash = (hash * 31 + item.id) & 2147483647;
            gpa.destroy(item);
        }
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, hash });
    try stdout.interface.flush();
}
