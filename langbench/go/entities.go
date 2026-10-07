// N entities held inline in one slice, scanned 100 times to sum their hit
// points. N is the program's first argument.
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
	entities := make([]Entity, 0, n)
	for i := 1; i <= n; i++ {
		entities = append(entities, Entity{i, 0.5 * float64(i), 0.25 * float64(i), i / 3})
	}
	total := 0
	for pass := 1; pass <= 100; pass++ {
		for k := range entities {
			total += entities[k].hp
		}
	}
	fmt.Println(n, total)
}
