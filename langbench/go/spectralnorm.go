// The Benchmarks Game's spectral-norm: the largest singular value of an
// infinite matrix, by 10 rounds of the power method on its N by N corner.
// It prints the value in billionths, truncated. N is the program's first
// argument.
package main

import (
	"fmt"
	"math"
	"os"
	"strconv"
)

func entry(i, j int) float64 {
	return 1.0 / float64((i+j)*(i+j+1)/2+i+1)
}

func times(v []float64) []float64 {
	out := make([]float64, len(v))
	for i := range v {
		sum := 0.0
		for j, x := range v {
			sum = sum + entry(i, j)*x
		}
		out[i] = sum
	}
	return out
}

func timesTransposed(v []float64) []float64 {
	out := make([]float64, len(v))
	for i := range v {
		sum := 0.0
		for j, x := range v {
			sum = sum + entry(j, i)*x
		}
		out[i] = sum
	}
	return out
}

func timesBoth(v []float64) []float64 {
	return timesTransposed(times(v))
}

func main() {
	n := 100
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	u := make([]float64, n)
	for i := range u {
		u[i] = 1.0
	}
	var v []float64
	for round := 0; round < 10; round++ {
		v = timesBoth(u)
		u = timesBoth(v)
	}
	vBv, vv := 0.0, 0.0
	for k := 0; k < n; k++ {
		vBv = vBv + u[k]*v[k]
		vv = vv + v[k]*v[k]
	}
	fmt.Println(n, int64(math.Sqrt(vBv/vv)*1e9))
}
