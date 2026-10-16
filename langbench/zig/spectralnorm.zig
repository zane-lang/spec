// The Benchmarks Game's spectral-norm: the largest singular value of an
// infinite matrix, by 10 rounds of the power method on its N by N corner.
// It prints the value in billionths, truncated. N is the program's first
// argument.
const std = @import("std");

fn entry(i: usize, j: usize) f64 {
    return 1.0 / @as(f64, @floatFromInt((i + j) * (i + j + 1) / 2 + i + 1));
}

fn times(v: []const f64, out: []f64) void {
    for (out, 0..) |*o, i| {
        var sum: f64 = 0.0;
        for (v, 0..) |x, j| sum = sum + entry(i, j) * x;
        o.* = sum;
    }
}

fn timesTransposed(v: []const f64, out: []f64) void {
    for (out, 0..) |*o, i| {
        var sum: f64 = 0.0;
        for (v, 0..) |x, j| sum = sum + entry(j, i) * x;
        o.* = sum;
    }
}

fn timesBoth(v: []const f64, out: []f64, tmp: []f64) void {
    times(v, tmp);
    timesTransposed(tmp, out);
}

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: usize = if (args.len > 1) std.fmt.parseInt(usize, args[1], 10) catch 100 else 100;
    const u = try gpa.alloc(f64, n);
    defer gpa.free(u);
    const v = try gpa.alloc(f64, n);
    defer gpa.free(v);
    const tmp = try gpa.alloc(f64, n);
    defer gpa.free(tmp);
    @memset(u, 1.0);
    for (0..10) |_| {
        timesBoth(u, v, tmp);
        timesBoth(v, u, tmp);
    }
    var vbv: f64 = 0.0;
    var vv: f64 = 0.0;
    for (u, v) |a, b| {
        vbv = vbv + a * b;
        vv = vv + b * b;
    }
    const norm: i64 = @intFromFloat(@sqrt(vbv / vv) * 1e9);
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, norm });
    try stdout.interface.flush();
}
