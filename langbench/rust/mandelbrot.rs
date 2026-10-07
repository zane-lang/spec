// The Mandelbrot set on an N by N grid: how many points stay bounded for 50
// iterations. N is the program's first argument.

fn bounded(cr: f64, ci: f64) -> bool {
    let (mut zr, mut zi) = (0.0f64, 0.0f64);
    for _ in 1..=50 {
        let tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if 4.0 < zr * zr + zi * zi {
            return false;
        }
    }
    true
}

fn main() {
    let n: i64 = std::env::args()
        .nth(1)
        .and_then(|a| a.parse().ok())
        .unwrap_or(200);
    let mut inside = 0;
    for y in 0..n {
        for x in 0..n {
            let cr = 2.0 * x as f64 / n as f64 - 1.5;
            let ci = 2.0 * y as f64 / n as f64 - 1.0;
            if bounded(cr, ci) {
                inside += 1;
            }
        }
    }
    println!("{n} {inside}");
}
