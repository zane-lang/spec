// The Benchmarks Game's fannkuch-redux: every permutation of N items, each
// flipped by reversing its first k + 1 items while the first item k is not 0.
// N is the program's first argument.
const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: usize = if (args.len > 1) std.fmt.parseInt(usize, args[1], 10) catch 7 else 7;
    var perm1: [32]usize = undefined;
    var perm: [32]usize = undefined;
    var count: [32]usize = undefined;
    for (0..n) |i| perm1[i] = i;
    var checksum: i64 = 0;
    var max_flips: i64 = 0;
    var sign: i64 = 1;
    var r = n;
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    while (true) {
        while (r != 1) : (r -= 1) count[r - 1] = r;
        @memcpy(perm[0..n], perm1[0..n]);
        var flips: i64 = 0;
        while (perm[0] != 0) : (flips += 1) {
            std.mem.reverse(usize, perm[0 .. perm[0] + 1]);
        }
        max_flips = @max(max_flips, flips);
        checksum += sign * flips;
        sign = -sign;
        while (true) {
            if (r == n) {
                try stdout.interface.print("{d} {d}\n", .{ checksum, max_flips });
                try stdout.interface.flush();
                return;
            }
            std.mem.rotate(usize, perm1[0 .. r + 1], 1);
            count[r] -= 1;
            if (count[r] > 0) break;
            r += 1;
        }
    }
}
