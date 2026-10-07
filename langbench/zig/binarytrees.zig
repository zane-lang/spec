// The Benchmarks Game's binary-trees: perfect binary trees built, walked and
// torn down, many small ones and one that lives through the run.
const std = @import("std");

const Node = struct {
    left: ?*Node,
    right: ?*Node,
};

fn make(gpa: std.mem.Allocator, depth: u32) !*Node {
    const node = try gpa.create(Node);
    if (depth == 0) {
        node.* = .{ .left = null, .right = null };
    } else {
        node.* = .{ .left = try make(gpa, depth - 1), .right = try make(gpa, depth - 1) };
    }
    return node;
}

fn check(node: *const Node) u64 {
    const left = node.left orelse return 1;
    return 1 + check(left) + check(node.right.?);
}

fn release(gpa: std.mem.Allocator, node: *Node) void {
    if (node.left) |left| {
        release(gpa, left);
        release(gpa, node.right.?);
    }
    gpa.destroy(node);
}

pub fn main(init: std.process.Init) !void {
    // In ReleaseSafe `init.gpa` is the leak-checking debug allocator; a
    // release program asks for the general-purpose one.
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: u32 = if (args.len > 1) std.fmt.parseInt(u32, args[1], 10) catch 10 else 10;
    var buffer: [256]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    const out = &stdout.interface;

    const max_depth = @max(n, 6);
    const stretch = max_depth + 1;
    const stretch_tree = try make(gpa, stretch);
    try out.print("stretch {d} {d}\n", .{ stretch, check(stretch_tree) });
    release(gpa, stretch_tree);

    const long_lived = try make(gpa, max_depth);
    var depth: u32 = 4;
    while (depth <= max_depth) : (depth += 2) {
        const iterations = @as(u64, 1) << @intCast(max_depth - depth + 4);
        var total: u64 = 0;
        for (0..iterations) |_| {
            const tree = try make(gpa, depth);
            total += check(tree);
            release(gpa, tree);
        }
        try out.print("{d} {d} {d}\n", .{ iterations, depth, total });
    }
    try out.print("long {d} {d}\n", .{ max_depth, check(long_lived) });
    release(gpa, long_lived);
    try out.flush();
}
