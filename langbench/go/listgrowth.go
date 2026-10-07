// A growable list of entities, filled by appending N items one at a time,
// built and dropped 20 times. N is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

type Entity struct {
	id   int
	x, y float64
	hp   int
}

func main() {
	n := 100000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	total := 0
	for round := 1; round <= 20; round++ {
		var list []Entity
		for i := 1; i <= n; i++ {
			list = append(list, Entity{i, 0.5 * float64(i), 0.25 * float64(i), i / 3})
		}
		total += list[len(list)-1].hp + len(list)
	}
	fmt.Println(n, total)
}
