// N entities held inline in one vector, split into four shards, each shard
// summed on its own thread, 100 times over. N is the program's first
// argument.

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
    let n: usize = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(100000);
    let entities: Vec<Entity> = (1..=n as i64)
        .map(|i| Entity { id: i, x: 1.1 * i as f64, y: 2.2 * i as f64, hp: i % 100 + 1 })
        .collect();
    let mut total: i64 = 0;
    for _ in 0..100 {
        total += std::thread::scope(|scope| {
            let shards: Vec<_> = (0..4)
                .map(|s| {
                    let shard = &entities[n * s / 4..n * (s + 1) / 4];
                    scope.spawn(move || shard.iter().map(|e| e.hp).sum::<i64>())
                })
                .collect();
            shards.into_iter().map(|h| h.join().unwrap()).sum::<i64>()
        });
    }
    println!("{n} {total}");
}
