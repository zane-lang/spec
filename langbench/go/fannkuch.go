// The Benchmarks Game's fannkuch-redux: every permutation of N items, each
// flipped by reversing its first k + 1 items while the first item k is not 0.
// N is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

func main() {
	n := 7
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	perm1, perm, count := make([]int, n), make([]int, n), make([]int, n)
	for i := range perm1 {
		perm1[i] = i
	}
	checksum, maxFlips, sign, r := 0, 0, 1, n
	for {
		for ; r != 1; r-- {
			count[r-1] = r
		}
		copy(perm, perm1)
		flips := 0
		for k := perm[0]; k != 0; k = perm[0] {
			for i, j := 0, k; i < j; i, j = i+1, j-1 {
				perm[i], perm[j] = perm[j], perm[i]
			}
			flips++
		}
		maxFlips = max(maxFlips, flips)
		checksum += sign * flips
		sign = -sign
		for {
			if r == n {
				fmt.Println(checksum, maxFlips)
				return
			}
			first := perm1[0]
			copy(perm1[:r], perm1[1:r+1])
			perm1[r] = first
			count[r]--
			if count[r] > 0 {
				break
			}
			r++
		}
	}
}
