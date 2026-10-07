// The primes up to N, counted by trial division against a table of small
// primes built with a sieve. N is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

func primesBelow(bound int) []int {
	composite := make([]bool, bound)
	var primes []int
	for p := 2; p < bound; p++ {
		if composite[p] {
			continue
		}
		primes = append(primes, p)
		for m := p * p; m < bound; m += p {
			composite[m] = true
		}
	}
	return primes
}

func isPrime(k int, table []int) bool {
	for _, p := range table {
		if k < p*p {
			return true
		}
		if k%p == 0 {
			return false
		}
	}
	return true
}

func main() {
	table := primesBelow(4000)
	n := 100000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	count := 0
	for k := 2; k <= n; k++ {
		if isPrime(k, table) {
			count++
		}
	}
	fmt.Println(n, count)
}
