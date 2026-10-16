// A game loop over F frames: each frame spawns 30 entities, kills the 20
// oldest, and updates the rest, removing those whose hit points run out.
// The list keeps the entities in the order they spawned. F is the
// program's first argument.
const std = @import("std");

const Entity = struct {
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
    const frames: i64 = if (args.len > 1) std.fmt.parseInt(i64, args[1], 10) catch 500 else 500;
    var rng: Rng = .{};
    var entities: std.ArrayList(*Entity) = .empty;
    defer {
        for (entities.items) |e| gpa.destroy(e);
        entities.deinit(gpa);
    }
    var total: i64 = 0;
    var frame: i64 = 1;
    while (frame <= frames) : (frame += 1) {
        for (0..30) |_| {
            const x: f64 = @floatFromInt(rng.below(1000));
            const y: f64 = @floatFromInt(rng.below(1000));
            const id = rng.below(100);
            const hp = 50 + rng.below(50);
            const e = try gpa.create(Entity);
            e.* = .{ .id = id, .x = x, .y = y, .hp = hp };
            try entities.append(gpa, e);
        }
        const kills = @min(20, entities.items.len);
        for (0..kills) |_| gpa.destroy(entities.orderedRemove(0));
        var k: usize = 0;
        for (0..entities.items.len) |_| {
            const e = entities.items[k];
            e.x = e.x + @as(f64, @floatFromInt(e.id)) * 0.1;
            e.y = e.y + @as(f64, @floatFromInt(e.hp)) * 0.05;
            e.hp = e.hp - 1;
            if (e.hp < 1) {
                gpa.destroy(entities.orderedRemove(k));
            } else {
                total += e.hp;
                k += 1;
            }
        }
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d} {d}\n", .{ frames, entities.items.len, total });
    try stdout.interface.flush();
}
