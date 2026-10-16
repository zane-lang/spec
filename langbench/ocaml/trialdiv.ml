(* The primes up to N, counted by trial division against a table of small
   primes built with a sieve. N is the program's first argument. *)

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let primes_below bound =
  let composite = Array.make bound false in
  let primes = Dynarray.create () in
  for p = 2 to bound - 1 do
    if not composite.(p) then begin
      Dynarray.add_last primes p;
      let m = ref (p * p) in
      while !m < bound do
        composite.(!m) <- true;
        m := !m + p
      done
    end
  done;
  Dynarray.to_array primes

let is_prime table k =
  let rec go i =
    if i = Array.length table then true
    else
      let p = table.(i) in
      if k < p * p then true else if k mod p = 0 then false else go (i + 1)
  in
  go 0

let () =
  let table = primes_below 4000 in
  let n = size 100000 in
  let count = ref 0 in
  for k = 2 to n do
    if is_prime table k then incr count
  done;
  Printf.printf "%d %d\n" n !count
