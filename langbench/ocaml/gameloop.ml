(* A game loop over F frames: each frame spawns 30 entities, kills the 20
   oldest, and updates the rest, removing those whose hit points run out.
   The growable array keeps the entities in the order they spawned. F is
   the program's first argument. *)

type entity = { id : int; mutable x : float; mutable y : float; mutable hp : int }

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let state = ref 42

let below bound =
  state := ((!state * 1103515245) + 12345) land 2147483647;
  !state mod bound

(* Removes the element at [k], shifting the later ones down. *)
let remove_at a k =
  Dynarray.blit ~src:a ~src_pos:(k + 1) ~dst:a ~dst_pos:k ~len:(Dynarray.length a - k - 1);
  Dynarray.remove_last a

let () =
  let frames = size 500 in
  let entities = Dynarray.create () in
  let total = ref 0 in
  for _ = 1 to frames do
    for _ = 1 to 30 do
      let x = float_of_int (below 1000) in
      let y = float_of_int (below 1000) in
      let id = below 100 in
      let hp = 50 + below 50 in
      Dynarray.add_last entities { id; x; y; hp }
    done;
    for _ = 1 to min 20 (Dynarray.length entities) do
      remove_at entities 0
    done;
    let k = ref 0 in
    for _ = 1 to Dynarray.length entities do
      let e = Dynarray.get entities !k in
      e.x <- e.x +. (float_of_int e.id *. 0.1);
      e.y <- e.y +. (float_of_int e.hp *. 0.05);
      e.hp <- e.hp - 1;
      if e.hp < 1 then remove_at entities !k
      else begin
        total := !total + e.hp;
        incr k
      end
    done
  done;
  Printf.printf "%d %d %d\n" frames (Dynarray.length entities) !total
