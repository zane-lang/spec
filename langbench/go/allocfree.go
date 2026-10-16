// N small objects allocated one by one into a slice, then released in
// random order, 10 times over. N is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

type Item struct {
	id   int
	x, y float64
	hp   int
}

type Rng struct{ state int }

func (r *Rng) below(bound int) int {
	r.state = (r.state*1103515245 + 12345) & 2147483647
	return r.state % bound
}

func main() {
	n := 10000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	rng := Rng{42}
	hash := 0
	for round := 1; round <= 10; round++ {
		var items []*Item
		for i := 1; i <= n; i++ {
			items = append(items, &Item{i, 0.5 * float64(i), 0.25 * float64(i), i / 3})
		}
		for k := 1; k <= n; k++ {
			index := rng.below(len(items))
			item := items[index]
			last := len(items) - 1
			items[index] = items[last]
			items[last] = nil
			items = items[:last]
			hash = (hash*31 + item.id) & 2147483647
		}
	}
	fmt.Println(n, hash)
}
