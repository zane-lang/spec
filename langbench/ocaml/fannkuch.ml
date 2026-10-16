(* The Benchmarks Game's fannkuch-redux: every permutation of N items, each
   flipped by reversing its first k + 1 items while the first item k is not 0.
   N is the program's first argument. *)

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let () =
  let n = size 7 in
  let perm1 = Array.init n Fun.id in
  let perm = Array.make n 0 and count = Array.make n 0 in
  let checksum = ref 0 and max_flips = ref 0 and sign = ref 1 in
  let r = ref n in
  let finished = ref false in
  while not !finished do
    while !r <> 1 do
      count.(!r - 1) <- !r;
      decr r
    done;
    Array.blit perm1 0 perm 0 n;
    let flips = ref 0 in
    while perm.(0) <> 0 do
      let i = ref 0 and j = ref perm.(0) in
      while !i < !j do
        let t = perm.(!i) in
        perm.(!i) <- perm.(!j);
        perm.(!j) <- t;
        incr i;
        decr j
      done;
      incr flips
    done;
    if !max_flips < !flips then max_flips := !flips;
    checksum := !checksum + (!sign * !flips);
    sign := - !sign;
    let stepped = ref false in
    while not (!stepped || !finished) do
      if !r = n then finished := true
      else begin
        let first = perm1.(0) in
        for i = 0 to !r - 1 do
          perm1.(i) <- perm1.(i + 1)
        done;
        perm1.(!r) <- first;
        count.(!r) <- count.(!r) - 1;
        if count.(!r) > 0 then stepped := true else incr r
      end
    done
  done;
  Printf.printf "%d %d\n" !checksum !max_flips
