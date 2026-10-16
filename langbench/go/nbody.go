// The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
// N steps of 0.01 days. It prints the system's energy before and after, in
// billionths, truncated. N is the program's first argument.
package main

import (
	"fmt"
	"math"
	"os"
	"strconv"
)

type Body struct {
	x, y, z, vx, vy, vz, mass float64
}

var (
	pi        = 3.141592653589793
	solarMass = 4.0 * pi * pi
	year      = 365.24
)

func start() []Body {
	bodies := []Body{
		{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, solarMass},
		{4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
			0.00166007664274403694 * year, 0.00769901118419740425 * year, -0.0000690460016972063023 * year,
			0.000954791938424326609 * solarMass},
		{8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
			-0.00276742510726862411 * year, 0.00499852801234917238 * year, 0.0000230417297573763929 * year,
			0.000285885980666130812 * solarMass},
		{12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
			0.00296460137564761618 * year, 0.00237847173959480950 * year, -0.0000296589568540237556 * year,
			0.0000436624404335156298 * solarMass},
		{15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
			0.00268067772490389322 * year, 0.00162824170038242295 * year, -0.0000951592254519715870 * year,
			0.0000515138902046611451 * solarMass},
	}
	px, py, pz := 0.0, 0.0, 0.0
	for _, b := range bodies {
		px = px + b.vx*b.mass
		py = py + b.vy*b.mass
		pz = pz + b.vz*b.mass
	}
	bodies[0].vx = -px / solarMass
	bodies[0].vy = -py / solarMass
	bodies[0].vz = -pz / solarMass
	return bodies
}

func advance(bodies []Body, dt float64) {
	for i := range bodies {
		bi := &bodies[i]
		for j := i + 1; j < len(bodies); j++ {
			bj := &bodies[j]
			dx := bi.x - bj.x
			dy := bi.y - bj.y
			dz := bi.z - bj.z
			d2 := dx*dx + dy*dy + dz*dz
			mag := dt / (d2 * math.Sqrt(d2))
			bi.vx = bi.vx - dx*bj.mass*mag
			bi.vy = bi.vy - dy*bj.mass*mag
			bi.vz = bi.vz - dz*bj.mass*mag
			bj.vx = bj.vx + dx*bi.mass*mag
			bj.vy = bj.vy + dy*bi.mass*mag
			bj.vz = bj.vz + dz*bi.mass*mag
		}
	}
	for k := range bodies {
		b := &bodies[k]
		b.x = b.x + dt*b.vx
		b.y = b.y + dt*b.vy
		b.z = b.z + dt*b.vz
	}
}

func energy(bodies []Body) float64 {
	e := 0.0
	for i, b := range bodies {
		e = e + 0.5*b.mass*(b.vx*b.vx+b.vy*b.vy+b.vz*b.vz)
		for j := i + 1; j < len(bodies); j++ {
			dx := b.x - bodies[j].x
			dy := b.y - bodies[j].y
			dz := b.z - bodies[j].z
			e = e - b.mass*bodies[j].mass/math.Sqrt(dx*dx+dy*dy+dz*dz)
		}
	}
	return e
}

func main() {
	n := 1000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	bodies := start()
	before := energy(bodies)
	for step := 1; step <= n; step++ {
		advance(bodies, 0.01)
	}
	after := energy(bodies)
	fmt.Println(n, int64(before*1e9), int64(after*1e9))
}
