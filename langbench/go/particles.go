// A particle system over F frames: each frame spawns a burst of 60
// particles with 10 to 30 frames to live, then moves the living and
// expires the rest. Order does not matter, so an expired particle's slot
// takes the last one. F is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

type Particle struct {
	x, y, vx, vy float64
	ttl, color   int
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
	var particles []*Particle
	total := 0
	for frame := 1; frame <= frames; frame++ {
		for s := 0; s < 60; s++ {
			x := float64(rng.below(800))
			y := float64(rng.below(600))
			vx := float64(rng.below(11) - 5)
			vy := float64(rng.below(11) - 5)
			ttl := 10 + rng.below(21)
			color := rng.below(8)
			particles = append(particles, &Particle{x, y, vx, vy, ttl, color})
		}
		sum := 0.0
		k := 0
		for u := len(particles); u > 0; u-- {
			p := particles[k]
			p.ttl = p.ttl - 1
			if p.ttl < 1 {
				last := len(particles) - 1
				particles[k] = particles[last]
				particles[last] = nil
				particles = particles[:last]
			} else {
				p.x = p.x + p.vx
				p.y = p.y + p.vy
				sum = sum + p.x
				k++
			}
		}
		total += int(sum)
	}
	fmt.Println(frames, len(particles), total)
}
