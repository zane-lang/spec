// A game loop over F frames: each frame spawns 30 entities, kills the 20
// oldest, and updates the rest, removing those whose hit points run out.
// The slice keeps the entities in the order they spawned. F is the
// program's first argument.
package main

import (
	"fmt"
	"os"
	"slices"
	"strconv"
)

type Entity struct {
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
	frames := 500
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			frames = v
		}
	}
	rng := Rng{42}
	var entities []*Entity
	total := 0
	for frame := 1; frame <= frames; frame++ {
		for s := 0; s < 30; s++ {
			x := float64(rng.below(1000))
			y := float64(rng.below(1000))
			id := rng.below(100)
			hp := 50 + rng.below(50)
			entities = append(entities, &Entity{id, x, y, hp})
		}
		kills := min(20, len(entities))
		for d := 0; d < kills; d++ {
			entities = slices.Delete(entities, 0, 1)
		}
		k := 0
		for u := len(entities); u > 0; u-- {
			e := entities[k]
			e.x = e.x + float64(e.id)*0.1
			e.y = e.y + float64(e.hp)*0.05
			e.hp = e.hp - 1
			if e.hp < 1 {
				entities = slices.Delete(entities, k, k+1)
			} else {
				total += e.hp
				k++
			}
		}
	}
	fmt.Println(frames, len(entities), total)
}
