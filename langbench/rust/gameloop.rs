// A game loop over F frames: each frame spawns 30 entities, kills the 20
// oldest, and updates the rest, removing those whose hit points run out.
// The vector keeps the entities in the order they spawned. F is the
// program's first argument.

struct Entity {
    id: i64,
    x: f64,
    y: f64,
    hp: i64,
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
    let mut entities: Vec<Box<Entity>> = Vec::new();
    let mut total: i64 = 0;
    for _ in 0..frames {
        for _ in 0..30 {
            let x = rng.below(1000) as f64;
            let y = rng.below(1000) as f64;
            let id = rng.below(100);
            let hp = 50 + rng.below(50);
            entities.push(Box::new(Entity { id, x, y, hp }));
        }
        let kills = entities.len().min(20);
        for _ in 0..kills {
            entities.remove(0);
        }
        let mut k = 0;
        for _ in 0..entities.len() {
            let e = &mut entities[k];
            e.x = e.x + e.id as f64 * 0.1;
            e.y = e.y + e.hp as f64 * 0.05;
            e.hp = e.hp - 1;
            if e.hp < 1 {
                entities.remove(k);
            } else {
                total += e.hp;
                k += 1;
            }
        }
    }
    println!("{frames} {} {total}", entities.len());
}
