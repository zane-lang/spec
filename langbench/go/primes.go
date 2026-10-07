// The primes below a fixed bound, found with a sieve, counted and summed.
package main

import "fmt"

const bound = 20000

func main() {
	composite := make([]bool, bound)
	count, sum := 0, 0
	for p := 2; p < bound; p++ {
		if composite[p] {
			continue
		}
		count++
		sum += p
		for m := p * p; m < bound; m += p {
			composite[m] = true
		}
	}
	fmt.Println(count, sum)
}
