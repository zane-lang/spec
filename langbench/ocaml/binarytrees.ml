(* The Benchmarks Game's binary-trees: perfect binary trees built, walked
   and torn down, many small ones and one that lives through the run. *)

type tree = Leaf | Node of tree * tree

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let rec make depth =
  if depth = 0 then Leaf else Node (make (depth - 1), make (depth - 1))

let rec check = function
  | Leaf -> 1
  | Node (left, right) -> 1 + check left + check right

let () =
  let n = size 10 in
  let max_depth = max n 6 in
  let stretch = max_depth + 1 in
  Printf.printf "stretch %d %d\n" stretch (check (make stretch));
  let long_lived = make max_depth in
  let depth = ref 4 in
  while !depth <= max_depth do
    let iterations = 1 lsl (max_depth - !depth + 4) in
    let total = ref 0 in
    for _ = 1 to iterations do
      total := !total + check (make !depth)
    done;
    Printf.printf "%d %d %d\n" iterations !depth !total;
    depth := !depth + 2
  done;
  Printf.printf "long %d %d\n" max_depth (check long_lived)
