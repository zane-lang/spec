// The primes below a fixed bound, counted and summed by a sieve the compiler
// must run while it compiles.

const BOUND: usize = 20000;

const fn sieve() -> (u64, u64) {
    let mut composite = [false; BOUND];
    let (mut count, mut sum) = (0u64, 0u64);
    let mut p = 2;
    while p < BOUND {
        if !composite[p] {
            count += 1;
            sum += p as u64;
            let mut m = p * p;
            while m < BOUND {
                composite[m] = true;
                m += p;
            }
        }
        p += 1;
    }
    (count, sum)
}

const RESULT: (u64, u64) = sieve();

fn main() {
    println!("{} {}", RESULT.0, RESULT.1);
}
