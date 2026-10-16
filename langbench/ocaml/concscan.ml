(* N entities in one array, split into four shards, each shard summed on
   its own domain, 100 times over. N is the program's first argument. *)

type entity = { id : int; x : float; y : float; hp : int }

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let shard_sum entities first last =
  let sum = ref 0 in
  for k = first to last - 1 do
    sum := !sum + entities.(k).hp
  done;
  !sum

let () =
  let n = size 100000 in
  let entities =
    Array.init n (fun k ->
        let i = k + 1 in
        { id = i; x = 1.1 *. float_of_int i; y = 2.2 *. float_of_int i; hp = (i mod 100) + 1 })
  in
  let total = ref 0 in
  for _ = 1 to 100 do
    let shards =
      List.init 4 (fun s ->
          Domain.spawn (fun () -> shard_sum entities (n * s / 4) (n * (s + 1) / 4)))
    in
    List.iter (fun shard -> total := !total + Domain.join shard) shards
  done;
  Printf.printf "%d %d\n" n !total
