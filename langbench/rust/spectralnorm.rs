// The Benchmarks Game's spectral-norm: the largest singular value of an
// infinite matrix, by 10 rounds of the power method on its N by N corner.
// It prints the value in billionths, truncated. N is the program's first
// argument.

fn entry(i: usize, j: usize) -> f64 {
    1.0 / ((i + j) * (i + j + 1) / 2 + i + 1) as f64
}

fn times(v: &[f64]) -> Vec<f64> {
    (0..v.len())
        .map(|i| v.iter().enumerate().fold(0.0, |sum, (j, &x)| sum + entry(i, j) * x))
        .collect()
}

fn times_transposed(v: &[f64]) -> Vec<f64> {
    (0..v.len())
        .map(|i| v.iter().enumerate().fold(0.0, |sum, (j, &x)| sum + entry(j, i) * x))
        .collect()
}

fn times_both(v: &[f64]) -> Vec<f64> {
    times_transposed(&times(v))
}

fn main() {
    let n: usize = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(100);
    let mut u = vec![1.0; n];
    let mut v = Vec::new();
    for _ in 0..10 {
        v = times_both(&u);
        u = times_both(&v);
    }
    let (mut vbv, mut vv) = (0.0, 0.0);
    for k in 0..n {
        vbv = vbv + u[k] * v[k];
        vv = vv + v[k] * v[k];
    }
    println!("{n} {}", ((vbv / vv).sqrt() * 1e9) as i64);
}
