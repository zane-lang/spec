// Prints one fixed number and exits: the cost of starting a Zig program.
const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const zero: u64 = 0;
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d}\n", .{zero});
    try stdout.interface.flush();
}
