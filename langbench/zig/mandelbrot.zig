// The Mandelbrot set on an N by N grid: how many points stay bounded for 50
// iterations. N is the program's first argument.
const std = @import("std");

fn bounded(cr: f64, ci: f64) bool {
    var zr: f64 = 0.0;
    var zi: f64 = 0.0;
    for (1..51) |_| {
        const tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if (4.0 < zr * zr + zi * zi) return false;
    }
    return true;
}

pub fn main(init: std.process.Init) !void {
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: u64 = if (args.len > 1) std.fmt.parseInt(u64, args[1], 10) catch 200 else 200;
    const size: f64 = @floatFromInt(n);
    var inside: u64 = 0;
    for (0..n) |y| {
        for (0..n) |x| {
            const cr = 2.0 * @as(f64, @floatFromInt(x)) / size - 1.5;
            const ci = 2.0 * @as(f64, @floatFromInt(y)) / size - 1.0;
            if (bounded(cr, ci)) inside += 1;
        }
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, inside });
    try stdout.interface.flush();
}
