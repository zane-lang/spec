// N small objects allocated one by one into a vector, then released in
// random order, 10 times over. N is the program's first argument.

struct Item {
    id: i64,
    #[allow(dead_code)]
    x: f64,
    #[allow(dead_code)]
    y: f64,
    #[allow(dead_code)]
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
    let n: i64 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(10000);
    let mut rng = Rng(42);
    let mut hash: i64 = 0;
    for _ in 0..10 {
        let mut items: Vec<Box<Item>> = Vec::new();
        for i in 1..=n {
            items.push(Box::new(Item { id: i, x: 0.5 * i as f64, y: 0.25 * i as f64, hp: i / 3 }));
        }
        for _ in 0..n {
            let index = rng.below(items.len() as i64) as usize;
            let item = items.swap_remove(index);
            hash = (hash * 31 + item.id) & 2147483647;
        }
    }
    println!("{n} {hash}");
}
