// N entities held inline in one slice, split into four shards, each shard
// summed on its own goroutine, 100 times over. N is the program's first
// argument.
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
)

type Entity struct {
	id   int
	x, y float64
	hp   int
}

func shardSum(entities []Entity) int {
	sum := 0
	for k := range entities {
		sum += entities[k].hp
	}
	return sum
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
		entities = append(entities, Entity{i, 1.1 * float64(i), 2.2 * float64(i), i%100 + 1})
	}
	total := 0
	for pass := 1; pass <= 100; pass++ {
		var sums [4]int
		var wg sync.WaitGroup
		for s := 0; s < 4; s++ {
			wg.Go(func() {
				sums[s] = shardSum(entities[n*s/4 : n*(s+1)/4])
			})
		}
		wg.Wait()
		for _, sum := range sums {
			total += sum
		}
	}
	fmt.Println(n, total)
}
