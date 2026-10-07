// The primes below a fixed bound, found with a sieve, counted and summed.
const std = @import("std");

const bound = 20000;

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const composite = try gpa.alloc(bool, bound);
    defer gpa.free(composite);
    @memset(composite, false);
    var count: u64 = 0;
    var sum: u64 = 0;
    for (2..bound) |p| {
        if (composite[p]) continue;
        count += 1;
        sum += p;
        var m = p * p;
        while (m < bound) : (m += p) composite[m] = true;
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ count, sum });
    try stdout.interface.flush();
}
