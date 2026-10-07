// The Benchmarks Game's fannkuch-redux: every permutation of N items, each
// flipped by reversing its first k + 1 items while the first item k is not 0.
// N is the program's first argument.

fn main() {
    let n: usize = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(7);
    let mut perm1: Vec<usize> = (0..n).collect();
    let mut perm = vec![0; n];
    let mut count = vec![0; n];
    let (mut checksum, mut max_flips, mut sign) = (0i64, 0, 1i64);
    let mut r = n;
    loop {
        while r != 1 {
            count[r - 1] = r;
            r -= 1;
        }
        perm.copy_from_slice(&perm1);
        let mut flips = 0;
        while perm[0] != 0 {
            let k = perm[0];
            perm[..=k].reverse();
            flips += 1;
        }
        max_flips = max_flips.max(flips);
        checksum += sign * flips as i64;
        sign = -sign;
        loop {
            if r == n {
                println!("{checksum} {max_flips}");
                return;
            }
            perm1[..=r].rotate_left(1);
            count[r] -= 1;
            if count[r] > 0 {
                break;
            }
            r += 1;
        }
    }
}
