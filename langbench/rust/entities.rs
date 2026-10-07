// N entities held inline in one vector, scanned 100 times to sum their hit
// points. N is the program's first argument.

struct Entity {
    #[allow(dead_code)]
    id: i64,
    #[allow(dead_code)]
    x: f64,
    #[allow(dead_code)]
    y: f64,
    hp: i64,
}

fn main() {
    let n: i64 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(100000);
    let entities: Vec<Entity> = (1..=n)
        .map(|i| Entity { id: i, x: 0.5 * i as f64, y: 0.25 * i as f64, hp: i / 3 })
        .collect();
    let mut total = 0i64;
    for _ in 1..=100 {
        total += entities.iter().map(|e| e.hp).sum::<i64>();
    }
    println!("{n} {total}");
}
