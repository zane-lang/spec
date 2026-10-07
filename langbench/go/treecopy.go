// A perfect binary tree N levels deep, copied whole 20 times; each copy is
// checked and dropped. N is the program's first argument.
package main

import (
	"fmt"
	"os"
	"strconv"
)

type Node struct {
	value       int
	left, right *Node
}

func build(depth, value int) *Node {
	if depth == 0 {
		return &Node{value: value}
	}
	return &Node{value, build(depth-1, 2*value), build(depth-1, 2*value+1)}
}

func (n *Node) clone() *Node {
	if n == nil {
		return nil
	}
	return &Node{n.value, n.left.clone(), n.right.clone()}
}

func (n *Node) sum() int {
	if n.left == nil {
		return n.value
	}
	return n.value + n.left.sum() + n.right.sum()
}

func main() {
	depth := 12
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			depth = v
		}
	}
	original := build(depth, 1)
	total := 0
	for round := 1; round <= 20; round++ {
		total += original.clone().sum()
	}
	fmt.Println(depth, total, original.sum())
}
