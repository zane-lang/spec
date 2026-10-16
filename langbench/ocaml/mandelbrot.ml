(* The Mandelbrot set on an N by N grid: how many points stay bounded for 50
   iterations. N is the program's first argument. *)

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let bounded cr ci =
  let rec go i zr zi =
    if i > 50 then true
    else
      let tr = (zr *. zr) -. (zi *. zi) +. cr in
      let zi = (2.0 *. zr *. zi) +. ci in
      let zr = tr in
      if 4.0 < (zr *. zr) +. (zi *. zi) then false else go (i + 1) zr zi
  in
  go 1 0.0 0.0

let () =
  let n = size 200 in
  let inside = ref 0 in
  for y = 0 to n - 1 do
    for x = 0 to n - 1 do
      let cr = (2.0 *. float_of_int x /. float_of_int n) -. 1.5 in
      let ci = (2.0 *. float_of_int y /. float_of_int n) -. 1.0 in
      if bounded cr ci then incr inside
    done
  done;
  Printf.printf "%d %d\n" n !inside
