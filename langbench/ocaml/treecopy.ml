(* A perfect binary tree N levels deep, copied whole 20 times; each copy is
   checked and dropped. N is the program's first argument. *)

type tree = Leaf of int | Node of int * tree * tree

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let rec make depth value =
  if depth = 0 then Leaf value
  else Node (value, make (depth - 1) (value * 2), make (depth - 1) ((value * 2) + 1))

let rec copy = function
  | Leaf value -> Leaf value
  | Node (value, left, right) -> Node (value, copy left, copy right)

let rec sum = function
  | Leaf value -> value
  | Node (value, left, right) -> value + sum left + sum right

let () =
  let depth = size 12 in
  let original = make depth 1 in
  let total = ref 0 in
  for _ = 1 to 20 do
    total := !total + sum (copy original)
  done;
  Printf.printf "%d %d %d\n" depth !total (sum original)
