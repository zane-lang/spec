// Trees whose nodes own a growable list of children: a complete four-way
// tree N levels deep, built, counted and torn down 10 times. N is the
// program's first argument.

struct Node {
    children: Vec<Node>,
}

fn grow(depth: u32) -> Node {
    let mut children = Vec::new();
    if depth > 0 {
        for _ in 0..4 {
            children.push(grow(depth - 1));
        }
    }
    Node { children }
}

fn count(node: &Node) -> u64 {
    1 + node.children.iter().map(count).sum::<u64>()
}

fn main() {
    let depth: u32 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(6);
    let total: u64 = (0..10).map(|_| count(&grow(depth))).sum();
    println!("{depth} {total}");
}
