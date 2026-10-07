// The Benchmarks Game's binary-trees: perfect binary trees built, walked and
// torn down, many small ones and one that lives through the run.

struct Node {
    children: Option<(Box<Node>, Box<Node>)>,
}

fn make(depth: u32) -> Box<Node> {
    let children = if depth > 0 {
        Some((make(depth - 1), make(depth - 1)))
    } else {
        None
    };
    Box::new(Node { children })
}

fn check(node: &Node) -> u64 {
    match &node.children {
        Some((left, right)) => 1 + check(left) + check(right),
        None => 1,
    }
}

fn main() {
    let n: u32 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(10);
    let max_depth = n.max(6);
    let stretch = max_depth + 1;
    println!("stretch {} {}", stretch, check(&make(stretch)));
    let long_lived = make(max_depth);
    for depth in (4..=max_depth).step_by(2) {
        let iterations = 1u64 << (max_depth - depth + 4);
        let total: u64 = (0..iterations).map(|_| check(&make(depth))).sum();
        println!("{iterations} {depth} {total}");
    }
    println!("long {} {}", max_depth, check(&long_lived));
}
