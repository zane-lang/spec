// A particle system over F frames: each frame spawns a burst of 60
// particles with 10 to 30 frames to live, then moves the living and
// expires the rest. Order does not matter, so an expired particle's slot
// takes the last one. F is the program's first argument.
const std = @import("std");

const Particle = struct {
    x: f64,
    y: f64,
    vx: f64,
    vy: f64,
    ttl: i64,
    color: i64,
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
    var particles: std.ArrayList(*Particle) = .empty;
    defer {
        for (particles.items) |p| gpa.destroy(p);
        particles.deinit(gpa);
    }
    var total: i64 = 0;
    var frame: i64 = 1;
    while (frame <= frames) : (frame += 1) {
        for (0..60) |_| {
            const x: f64 = @floatFromInt(rng.below(800));
            const y: f64 = @floatFromInt(rng.below(600));
            const vx: f64 = @floatFromInt(rng.below(11) - 5);
            const vy: f64 = @floatFromInt(rng.below(11) - 5);
            const ttl = 10 + rng.below(21);
            const color = rng.below(8);
            const p = try gpa.create(Particle);
            p.* = .{ .x = x, .y = y, .vx = vx, .vy = vy, .ttl = ttl, .color = color };
            try particles.append(gpa, p);
        }
        var sum: f64 = 0.0;
        var k: usize = 0;
        for (0..particles.items.len) |_| {
            const p = particles.items[k];
            p.ttl = p.ttl - 1;
            if (p.ttl < 1) {
                gpa.destroy(particles.swapRemove(k));
            } else {
                p.x = p.x + p.vx;
                p.y = p.y + p.vy;
                sum = sum + p.x;
                k += 1;
            }
        }
        total += @as(i64, @intFromFloat(sum));
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d} {d}\n", .{ frames, particles.items.len, total });
    try stdout.interface.flush();
}
