// Trees whose nodes own a growable list of children: a complete four-way
// tree N levels deep, built, counted and torn down 10 times. N is the
// program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

type Node struct {
	children []*Node
}

func grow(depth int) *Node {
	node := &Node{}
	if depth > 0 {
		for k := 0; k < 4; k++ {
			node.children = append(node.children, grow(depth-1))
		}
	}
	return node
}

func count(node *Node) int {
	total := 1
	for _, child := range node.children {
		total += count(child)
	}
	return total
}

func main() {
	depth := 6
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			depth = v
		}
	}
	total := 0
	for round := 1; round <= 10; round++ {
		total += count(grow(depth))
	}
	fmt.Println(depth, total)
}
