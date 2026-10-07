// The Mandelbrot set on an N by N grid: how many points stay bounded for 50
// iterations. N is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

func bounded(cr, ci float64) bool {
	zr, zi := 0.0, 0.0
	for i := 1; i <= 50; i++ {
		tr := zr*zr - zi*zi + cr
		zi = 2.0*zr*zi + ci
		zr = tr
		if 4.0 < zr*zr+zi*zi {
			return false
		}
	}
	return true
}

func main() {
	n := 200
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	inside := 0
	for y := 0; y < n; y++ {
		for x := 0; x < n; x++ {
			cr := 2.0*float64(x)/float64(n) - 1.5
			ci := 2.0*float64(y)/float64(n) - 1.0
			if bounded(cr, ci) {
				inside++
			}
		}
	}
	fmt.Println(n, inside)
}
