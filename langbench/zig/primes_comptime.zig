// The primes below a fixed bound, counted and summed by a sieve the compiler
// must run while it compiles.
const std = @import("std");

const bound = 20000;

fn sieve() [2]u64 {
    @setEvalBranchQuota(1_000_000);
    var composite = [_]bool{false} ** bound;
    var count: u64 = 0;
    var sum: u64 = 0;
    for (2..bound) |p| {
        if (composite[p]) continue;
        count += 1;
        sum += p;
        var m = p * p;
        while (m < bound) : (m += p) composite[m] = true;
    }
    return .{ count, sum };
}

pub fn main(init: std.process.Init) !void {
    const result = comptime sieve();
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ result[0], result[1] });
    try stdout.interface.flush();
}
