// The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
// N steps of 0.01 days. It prints the system's energy before and after, in
// billionths, truncated. N is the program's first argument.
const std = @import("std");

const Body = struct {
    x: f64,
    y: f64,
    z: f64,
    vx: f64,
    vy: f64,
    vz: f64,
    mass: f64,
};

const pi: f64 = 3.141592653589793;
const solar_mass: f64 = 4.0 * pi * pi;
const year: f64 = 365.24;

fn body(x: f64, y: f64, z: f64, vx: f64, vy: f64, vz: f64, mass: f64) Body {
    return .{ .x = x, .y = y, .z = z, .vx = vx, .vy = vy, .vz = vz, .mass = mass };
}

fn start() [5]Body {
    var bodies = [5]Body{
        body(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, solar_mass),
        body(4.84143144246472090, -1.16032004402742839, -0.103622044471123109, 0.00166007664274403694 * year, 0.00769901118419740425 * year, -0.0000690460016972063023 * year, 0.000954791938424326609 * solar_mass),
        body(8.34336671824457987, 4.12479856412430479, -0.403523417114321381, -0.00276742510726862411 * year, 0.00499852801234917238 * year, 0.0000230417297573763929 * year, 0.000285885980666130812 * solar_mass),
        body(12.8943695621391310, -15.1111514016986312, -0.223307578892655734, 0.00296460137564761618 * year, 0.00237847173959480950 * year, -0.0000296589568540237556 * year, 0.0000436624404335156298 * solar_mass),
        body(15.3796971148509165, -25.9193146099879641, 0.179258772950371181, 0.00268067772490389322 * year, 0.00162824170038242295 * year, -0.0000951592254519715870 * year, 0.0000515138902046611451 * solar_mass),
    };
    var px: f64 = 0.0;
    var py: f64 = 0.0;
    var pz: f64 = 0.0;
    for (bodies) |b| {
        px = px + b.vx * b.mass;
        py = py + b.vy * b.mass;
        pz = pz + b.vz * b.mass;
    }
    bodies[0].vx = -px / solar_mass;
    bodies[0].vy = -py / solar_mass;
    bodies[0].vz = -pz / solar_mass;
    return bodies;
}

fn advance(bodies: *[5]Body, dt: f64) void {
    for (bodies, 0..) |*bi, i| {
        for (bodies[i + 1 ..]) |*bj| {
            const dx = bi.x - bj.x;
            const dy = bi.y - bj.y;
            const dz = bi.z - bj.z;
            const d2 = dx * dx + dy * dy + dz * dz;
            const mag = dt / (d2 * @sqrt(d2));
            bi.vx = bi.vx - dx * bj.mass * mag;
            bi.vy = bi.vy - dy * bj.mass * mag;
            bi.vz = bi.vz - dz * bj.mass * mag;
            bj.vx = bj.vx + dx * bi.mass * mag;
            bj.vy = bj.vy + dy * bi.mass * mag;
            bj.vz = bj.vz + dz * bi.mass * mag;
        }
    }
    for (bodies) |*b| {
        b.x = b.x + dt * b.vx;
        b.y = b.y + dt * b.vy;
        b.z = b.z + dt * b.vz;
    }
}

fn energy(bodies: *const [5]Body) f64 {
    var e: f64 = 0.0;
    for (bodies, 0..) |b, i| {
        e = e + 0.5 * b.mass * (b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
        for (bodies[i + 1 ..]) |bj| {
            const dx = b.x - bj.x;
            const dy = b.y - bj.y;
            const dz = b.z - bj.z;
            e = e - b.mass * bj.mass / @sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    return e;
}

pub fn main(init: std.process.Init) !void {
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    const n: i64 = if (args.len > 1) std.fmt.parseInt(i64, args[1], 10) catch 1000 else 1000;
    var bodies = start();
    const before = energy(&bodies);
    var step: i64 = 1;
    while (step <= n) : (step += 1) advance(&bodies, 0.01);
    const after = energy(&bodies);
    const b: i64 = @intFromFloat(before * 1e9);
    const a: i64 = @intFromFloat(after * 1e9);
    var buffer: [64]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(init.io, &buffer);
    try stdout.interface.print("{d} {d} {d}\n", .{ n, b, a });
    try stdout.interface.flush();
}
