(* Trees whose nodes own a growable array of children: a complete four-way
   tree N levels deep, built, counted and torn down 10 times. N is the
   program's first argument. *)

type node = { children : node Dynarray.t }

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let rec grow depth =
  let node = { children = Dynarray.create () } in
  if depth > 0 then
    for _ = 1 to 4 do
      Dynarray.add_last node.children (grow (depth - 1))
    done;
  node

let rec count node =
  Dynarray.fold_left (fun total child -> total + count child) 1 node.children

let () =
  let depth = size 6 in
  let total = ref 0 in
  for _ = 1 to 10 do
    total := !total + count (grow depth)
  done;
  Printf.printf "%d %d\n" depth !total
