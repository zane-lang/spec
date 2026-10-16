(* A growable array of entities, filled by appending N items one at a time,
   built and dropped 20 times. N is the program's first argument. *)

type entity = { id : int; x : float; y : float; hp : int }

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let () =
  let n = size 100000 in
  let total = ref 0 in
  for _ = 1 to 20 do
    let list = Dynarray.create () in
    for i = 1 to n do
      Dynarray.add_last list
        { id = i; x = 0.5 *. float_of_int i; y = 0.25 *. float_of_int i; hp = i / 3 }
    done;
    total := !total + (Dynarray.get_last list).hp + Dynarray.length list
  done;
  Printf.printf "%d %d\n" n !total
