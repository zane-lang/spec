// The primes up to N, counted by trial division against a table of small
// primes built with a sieve. N is the program's first argument.

fn primes_below(bound: usize) -> Vec<u64> {
    let mut composite = vec![false; bound];
    let mut primes = Vec::new();
    for p in 2..bound {
        if composite[p] {
            continue;
        }
        primes.push(p as u64);
        for m in (p * p..bound).step_by(p) {
            composite[m] = true;
        }
    }
    primes
}

fn is_prime(k: u64, table: &[u64]) -> bool {
    for &p in table {
        if k < p * p {
            return true;
        }
        if k % p == 0 {
            return false;
        }
    }
    true
}

fn main() {
    let table = primes_below(4000);
    let n: u64 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(100000);
    let count = (2..=n).filter(|&k| is_prime(k, &table)).count();
    println!("{n} {count}");
}
