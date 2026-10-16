// A particle system over F frames: each frame spawns a burst of 60
// particles with 10 to 30 frames to live, then moves the living and
// expires the rest. Order does not matter, so an expired particle's slot
// takes the last one. F is the program's first argument.

struct Particle {
    x: f64,
    y: f64,
    vx: f64,
    vy: f64,
    ttl: i64,
    #[allow(dead_code)]
    color: i64,
}

struct Rng(i64);

impl Rng {
    fn below(&mut self, bound: i64) -> i64 {
        self.0 = (self.0 * 1103515245 + 12345) & 2147483647;
        self.0 % bound
    }
}

fn main() {
    let frames: i64 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(500);
    let mut rng = Rng(42);
    let mut particles: Vec<Box<Particle>> = Vec::new();
    let mut total: i64 = 0;
    for _ in 0..frames {
        for _ in 0..60 {
            let x = rng.below(800) as f64;
            let y = rng.below(600) as f64;
            let vx = (rng.below(11) - 5) as f64;
            let vy = (rng.below(11) - 5) as f64;
            let ttl = 10 + rng.below(21);
            let color = rng.below(8);
            particles.push(Box::new(Particle { x, y, vx, vy, ttl, color }));
        }
        let mut sum = 0.0f64;
        let mut k = 0;
        for _ in 0..particles.len() {
            let p = &mut particles[k];
            p.ttl = p.ttl - 1;
            if p.ttl < 1 {
                particles.swap_remove(k);
            } else {
                p.x = p.x + p.vx;
                p.y = p.y + p.vy;
                sum = sum + p.x;
                k += 1;
            }
        }
        total += sum as i64;
    }
    println!("{frames} {} {total}", particles.len());
}
