(* N entities in one array, scanned 100 times to sum their hit points. An
   OCaml record that mixes ints and floats is its own block, so the array
   holds pointers to them. N is the program's first argument. *)

type entity = { id : int; x : float; y : float; hp : int }

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let () =
  let n = size 100000 in
  let entities =
    Array.init n (fun k ->
        let i = k + 1 in
        { id = i; x = 0.5 *. float_of_int i; y = 0.25 *. float_of_int i; hp = i / 3 })
  in
  let total = ref 0 in
  for _ = 1 to 100 do
    Array.iter (fun e -> total := !total + e.hp) entities
  done;
  Printf.printf "%d %d\n" n !total
