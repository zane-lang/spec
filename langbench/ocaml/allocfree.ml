(* N small objects allocated one by one into a growable array, then
   released in random order, 10 times over. N is the program's first
   argument. *)

type item = { id : int; x : float; y : float; hp : int }

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let state = ref 42

let below bound =
  state := ((!state * 1103515245) + 12345) land 2147483647;
  !state mod bound

let () =
  let n = size 10000 in
  let hash = ref 0 in
  for _ = 1 to 10 do
    let items = Dynarray.create () in
    for i = 1 to n do
      Dynarray.add_last items
        { id = i; x = 0.5 *. float_of_int i; y = 0.25 *. float_of_int i; hp = i / 3 }
    done;
    for _ = 1 to n do
      let index = below (Dynarray.length items) in
      let item = Dynarray.get items index in
      Dynarray.set items index (Dynarray.get_last items);
      Dynarray.remove_last items;
      hash := ((!hash * 31) + item.id) land 2147483647
    done
  done;
  Printf.printf "%d %d\n" n !hash
