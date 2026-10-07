// A perfect binary tree N levels deep, copied whole 20 times; each copy is
// checked and dropped. N is the program's first argument.
const std = @import("std");

const Node = struct {
    value: i64,
    left: ?*Node,
    right: ?*Node,
};

fn make(gpa: std.mem.Allocator, depth: u32, value: i64) !*Node {
    const node = try gpa.create(Node);
    if (depth == 0) {
        node.* = .{ .value = value, .left = null, .right = null };
    } else {
        node.* = .{
            .value = value,
            .left = try make(gpa, depth - 1, 2 * value),
            .right = try make(gpa, depth - 1, 2 * value + 1),
        };
    }
    return node;
}

fn copy(gpa: std.mem.Allocator, node: *const Node) !*Node {
    const c = try gpa.create(Node);
    c.* = .{
        .value = node.value,
        .left = if (node.left) |left| try copy(gpa, left) else null,
        .right = if (node.right) |right| try copy(gpa, right) else null,
    };
    return c;
}

fn sum(node: *const Node) i64 {
    const left = node.left orelse return node.value;
    return node.value + sum(left) + sum(node.right.?);
}

fn release(gpa: std.mem.Allocator, node: *Node) void {
    if (node.left) |left| {
        release(gpa, left);
        release(gpa, node.right.?);
    }
    gpa.destroy(node);
}

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const depth: u32 = if (args.len > 1) std.fmt.parseInt(u32, args[1], 10) catch 12 else 12;
    const original = try make(gpa, depth, 1);
    var total: i64 = 0;
    for (1..21) |_| {
        const c = try copy(gpa, original);
        total += sum(c);
        release(gpa, c);
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d} {d}\n", .{ depth, total, sum(original) });
    try stdout.interface.flush();
    release(gpa, original);
}
