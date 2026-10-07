// A growable list of entities, filled by appending N items one at a time,
// built and dropped 20 times. N is the program's first argument.

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
    let mut total = 0i64;
    for _ in 1..=20 {
        let mut list = Vec::new();
        for i in 1..=n {
            list.push(Entity {
                id: i,
                x: 0.5 * i as f64,
                y: 0.25 * i as f64,
                hp: i / 3,
            });
        }
        total += list.last().unwrap().hp + list.len() as i64;
    }
    println!("{n} {total}");
}
