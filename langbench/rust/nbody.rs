// The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
// N steps of 0.01 days. It prints the system's energy before and after, in
// billionths, truncated. N is the program's first argument.

#[derive(Clone, Copy)]
struct Body {
    x: f64,
    y: f64,
    z: f64,
    vx: f64,
    vy: f64,
    vz: f64,
    mass: f64,
}

const PI: f64 = 3.141592653589793;
const SOLAR_MASS: f64 = 4.0 * PI * PI;
const YEAR: f64 = 365.24;

#[rustfmt::skip]
fn start() -> [Body; 5] {
    let body = |x, y, z, vx, vy, vz, mass| Body { x, y, z, vx, vy, vz, mass };
    let mut bodies = [
        body(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, SOLAR_MASS),
        body(4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
             0.00166007664274403694 * YEAR, 0.00769901118419740425 * YEAR, -0.0000690460016972063023 * YEAR,
             0.000954791938424326609 * SOLAR_MASS),
        body(8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
             -0.00276742510726862411 * YEAR, 0.00499852801234917238 * YEAR, 0.0000230417297573763929 * YEAR,
             0.000285885980666130812 * SOLAR_MASS),
        body(12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
             0.00296460137564761618 * YEAR, 0.00237847173959480950 * YEAR, -0.0000296589568540237556 * YEAR,
             0.0000436624404335156298 * SOLAR_MASS),
        body(15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
             0.00268067772490389322 * YEAR, 0.00162824170038242295 * YEAR, -0.0000951592254519715870 * YEAR,
             0.0000515138902046611451 * SOLAR_MASS),
    ];
    let (mut px, mut py, mut pz) = (0.0, 0.0, 0.0);
    for b in &bodies {
        px = px + b.vx * b.mass;
        py = py + b.vy * b.mass;
        pz = pz + b.vz * b.mass;
    }
    bodies[0].vx = -px / SOLAR_MASS;
    bodies[0].vy = -py / SOLAR_MASS;
    bodies[0].vz = -pz / SOLAR_MASS;
    bodies
}

fn advance(bodies: &mut [Body; 5], dt: f64) {
    for i in 0..bodies.len() {
        let (head, tail) = bodies.split_at_mut(i + 1);
        let bi = &mut head[i];
        for bj in tail.iter_mut() {
            let dx = bi.x - bj.x;
            let dy = bi.y - bj.y;
            let dz = bi.z - bj.z;
            let d2 = dx * dx + dy * dy + dz * dz;
            let mag = dt / (d2 * d2.sqrt());
            bi.vx = bi.vx - dx * bj.mass * mag;
            bi.vy = bi.vy - dy * bj.mass * mag;
            bi.vz = bi.vz - dz * bj.mass * mag;
            bj.vx = bj.vx + dx * bi.mass * mag;
            bj.vy = bj.vy + dy * bi.mass * mag;
            bj.vz = bj.vz + dz * bi.mass * mag;
        }
    }
    for b in bodies.iter_mut() {
        b.x = b.x + dt * b.vx;
        b.y = b.y + dt * b.vy;
        b.z = b.z + dt * b.vz;
    }
}

fn energy(bodies: &[Body; 5]) -> f64 {
    let mut e = 0.0;
    for (i, b) in bodies.iter().enumerate() {
        e = e + 0.5 * b.mass * (b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
        for bj in &bodies[i + 1..] {
            let dx = b.x - bj.x;
            let dy = b.y - bj.y;
            let dz = b.z - bj.z;
            e = e - b.mass * bj.mass / (dx * dx + dy * dy + dz * dz).sqrt();
        }
    }
    e
}

fn main() {
    let n: i64 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(1000);
    let mut bodies = start();
    let before = energy(&bodies);
    for _ in 0..n {
        advance(&mut bodies, 0.01);
    }
    let after = energy(&bodies);
    println!("{n} {} {}", (before * 1e9) as i64, (after * 1e9) as i64);
}
