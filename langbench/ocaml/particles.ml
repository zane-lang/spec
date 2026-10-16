(* A particle system over F frames: each frame spawns a burst of 60
   particles with 10 to 30 frames to live, then moves the living and
   expires the rest. Order does not matter, so an expired particle's slot
   takes the last one. F is the program's first argument. *)

type particle = {
  mutable x : float;
  mutable y : float;
  vx : float;
  vy : float;
  mutable ttl : int;
  color : int;
}

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let state = ref 42

let below bound =
  state := ((!state * 1103515245) + 12345) land 2147483647;
  !state mod bound

let () =
  let frames = size 500 in
  let particles = Dynarray.create () in
  let total = ref 0 in
  for _ = 1 to frames do
    for _ = 1 to 60 do
      let x = float_of_int (below 800) in
      let y = float_of_int (below 600) in
      let vx = float_of_int (below 11 - 5) in
      let vy = float_of_int (below 11 - 5) in
      let ttl = 10 + below 21 in
      let color = below 8 in
      Dynarray.add_last particles { x; y; vx; vy; ttl; color }
    done;
    let sum = ref 0.0 in
    let k = ref 0 in
    for _ = 1 to Dynarray.length particles do
      let p = Dynarray.get particles !k in
      p.ttl <- p.ttl - 1;
      if p.ttl < 1 then begin
        Dynarray.set particles !k (Dynarray.get_last particles);
        Dynarray.remove_last particles
      end
      else begin
        p.x <- p.x +. p.vx;
        p.y <- p.y +. p.vy;
        sum := !sum +. p.x;
        incr k
      end
    done;
    total := !total + int_of_float !sum
  done;
  Printf.printf "%d %d %d\n" frames (Dynarray.length particles) !total
