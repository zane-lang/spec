// A perfect binary tree N levels deep, copied whole 20 times; each copy is
// checked and dropped. N is the program's first argument.

#[derive(Clone)]
enum Tree {
    Leaf(i64),
    Node(i64, Box<Tree>, Box<Tree>),
}

fn make(depth: u32, value: i64) -> Tree {
    if depth == 0 {
        Tree::Leaf(value)
    } else {
        Tree::Node(
            value,
            Box::new(make(depth - 1, value * 2)),
            Box::new(make(depth - 1, value * 2 + 1)),
        )
    }
}

fn sum(tree: &Tree) -> i64 {
    match tree {
        Tree::Leaf(v) => *v,
        Tree::Node(v, left, right) => v + sum(left) + sum(right),
    }
}

fn main() {
    let depth: u32 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(12);
    let original = make(depth, 1);
    let mut total = 0;
    for _ in 1..=20 {
        let copy = original.clone();
        total += sum(&copy);
    }
    println!("{} {} {}", depth, total, sum(&original));
}
