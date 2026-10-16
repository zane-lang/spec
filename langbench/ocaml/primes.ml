(* The primes below a fixed bound, found with a sieve, counted and summed. *)

let bound = 20000

let () =
  let composite = Array.make bound false in
  let count = ref 0 and sum = ref 0 in
  for p = 2 to bound - 1 do
    if not composite.(p) then begin
      incr count;
      sum := !sum + p;
      let m = ref (p * p) in
      while !m < bound do
        composite.(!m) <- true;
        m := !m + p
      done
    end
  done;
  Printf.printf "%d %d\n" !count !sum
