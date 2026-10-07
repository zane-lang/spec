// The Benchmarks Game's binary-trees: perfect binary trees built, walked and
// torn down, many small ones and one that lives through the run.
package main

import (
	"fmt"
	"os"
	"strconv"
)

type Node struct {
	left, right *Node
}

func build(depth int) *Node {
	if depth == 0 {
		return &Node{}
	}
	return &Node{build(depth - 1), build(depth - 1)}
}

func (n *Node) check() int {
	if n.left == nil {
		return 1
	}
	return 1 + n.left.check() + n.right.check()
}

func main() {
	n := 10
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	maxDepth := max(n, 6)
	stretch := maxDepth + 1
	fmt.Println("stretch", stretch, build(stretch).check())
	longLived := build(maxDepth)
	for depth := 4; depth <= maxDepth; depth += 2 {
		iterations := 1 << (maxDepth - depth + 4)
		total := 0
		for i := 0; i < iterations; i++ {
			total += build(depth).check()
		}
		fmt.Println(iterations, depth, total)
	}
	fmt.Println("long", maxDepth, longLived.check())
}
