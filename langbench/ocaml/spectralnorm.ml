(* The Benchmarks Game's spectral-norm: the largest singular value of an
   infinite matrix, by 10 rounds of the power method on its N by N corner.
   It prints the value in billionths, truncated. N is the program's first
   argument. *)

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let entry i j = 1.0 /. float_of_int ((((i + j) * (i + j + 1)) / 2) + i + 1)

let times v =
  let n = Array.length v in
  Array.init n (fun i ->
      let sum = ref 0.0 in
      for j = 0 to n - 1 do
        sum := !sum +. (entry i j *. v.(j))
      done;
      !sum)

let times_transposed v =
  let n = Array.length v in
  Array.init n (fun i ->
      let sum = ref 0.0 in
      for j = 0 to n - 1 do
        sum := !sum +. (entry j i *. v.(j))
      done;
      !sum)

let times_both v = times_transposed (times v)

let () =
  let n = size 100 in
  let u = ref (Array.make n 1.0) and v = ref [||] in
  for _ = 1 to 10 do
    v := times_both !u;
    u := times_both !v
  done;
  let vbv = ref 0.0 and vv = ref 0.0 in
  for k = 0 to n - 1 do
    vbv := !vbv +. (!u.(k) *. !v.(k));
    vv := !vv +. (!v.(k) *. !v.(k))
  done;
  Printf.printf "%d %d\n" n (int_of_float (sqrt (!vbv /. !vv) *. 1e9))
