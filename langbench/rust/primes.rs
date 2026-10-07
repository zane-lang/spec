// The primes below a fixed bound, found with a sieve, counted and summed.

const BOUND: usize = 20000;

fn main() {
    let mut composite = vec![false; BOUND];
    let (mut count, mut sum) = (0u64, 0u64);
    for p in 2..BOUND {
        if composite[p] {
            continue;
        }
        count += 1;
        sum += p as u64;
        for m in (p * p..BOUND).step_by(p) {
            composite[m] = true;
        }
    }
    println!("{count} {sum}");
}
