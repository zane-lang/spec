// Trees whose nodes own a growable list of children: a complete four-way
// tree N levels deep, built, counted and torn down 10 times. N is the
// program's first argument.
const std = @import("std");

const Node = struct {
    children: std.ArrayList(*Node) = .empty,
};

fn grow(gpa: std.mem.Allocator, depth: u32) !*Node {
    const node = try gpa.create(Node);
    node.* = .{};
    if (depth > 0) {
        for (0..4) |_| try node.children.append(gpa, try grow(gpa, depth - 1));
    }
    return node;
}

fn count(node: *const Node) u64 {
    var total: u64 = 1;
    for (node.children.items) |child| total += count(child);
    return total;
}

fn release(gpa: std.mem.Allocator, node: *Node) void {
    for (node.children.items) |child| release(gpa, child);
    node.children.deinit(gpa);
    gpa.destroy(node);
}

pub fn main(init: std.process.Init) !void {
    const gpa = std.heap.smp_allocator;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const depth: u32 = if (args.len > 1) std.fmt.parseInt(u32, args[1], 10) catch 6 else 6;
    var total: u64 = 0;
    for (1..11) |_| {
        const tree = try grow(gpa, depth);
        total += count(tree);
        release(gpa, tree);
    }
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d}\n", .{ depth, total });
    try stdout.interface.flush();
}
