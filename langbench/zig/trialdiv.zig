// The primes up to N, counted by trial division against a table of small
// primes built with a sieve. N is the program's first argument.
const std = @import("std");

fn primesBelow(gpa: std.mem.Allocator, bound: usize) ![]u64 {
    const composite = try gpa.alloc(bool, bound);
    defer gpa.free(composite);
    @memset(composite, false);
    var primes: std.ArrayList(u64) = .empty;
    for (2..bound) |p| {
        if (composite[p]) continue;
        try primes.append(gpa, p);
        var m = p * p;
        while (m < bound) : (m += p) composite[m] = true;
    }
    return primes.toOwnedSlice(gpa);
}

fn isPrime(k: u64, table: []const u64) bool {
    for (table) |p| {
        if (k < p * p) return true;
        if (k % p == 0) return false;
    }
    return true;
}

pub fn main(init: std.process.Init) !void {
    const gpa = init.gpa;
    const table = try primesBelow(gpa, 4000);
    defer gpa.free(table);
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: u64 = if (args.len > 1) std.fmt.parseInt(u64, args[1], 10) catch 100000 else 100000;
    var count: u64 = 0;
    var k: u64 = 2;
    while (k <= n) : (k += 1) {
        if (isPrime(k, table)) count += 1;
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ n, count });
    try stdout.interface.flush();
}
