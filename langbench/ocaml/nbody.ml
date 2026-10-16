(* The Benchmarks Game's n-body: the Sun and the four gas giants, advanced
   N steps of 0.01 days. It prints the system's energy before and after, in
   billionths, truncated. N is the program's first argument. *)

type body = {
  mutable x : float;
  mutable y : float;
  mutable z : float;
  mutable vx : float;
  mutable vy : float;
  mutable vz : float;
  mass : float;
}

let size default =
  if Array.length Sys.argv > 1 then
    Option.value (int_of_string_opt Sys.argv.(1)) ~default
  else default

let pi = 3.141592653589793
let solar_mass = 4.0 *. pi *. pi
let year = 365.24
let body x y z vx vy vz mass = { x; y; z; vx; vy; vz; mass }

let start () =
  let bodies =
    [|
      body 0.0 0.0 0.0 0.0 0.0 0.0 solar_mass;
      body 4.84143144246472090 (-1.16032004402742839) (-0.103622044471123109)
        (0.00166007664274403694 *. year) (0.00769901118419740425 *. year)
        (-0.0000690460016972063023 *. year)
        (0.000954791938424326609 *. solar_mass);
      body 8.34336671824457987 4.12479856412430479 (-0.403523417114321381)
        (-0.00276742510726862411 *. year) (0.00499852801234917238 *. year)
        (0.0000230417297573763929 *. year)
        (0.000285885980666130812 *. solar_mass);
      body 12.8943695621391310 (-15.1111514016986312) (-0.223307578892655734)
        (0.00296460137564761618 *. year) (0.00237847173959480950 *. year)
        (-0.0000296589568540237556 *. year)
        (0.0000436624404335156298 *. solar_mass);
      body 15.3796971148509165 (-25.9193146099879641) 0.179258772950371181
        (0.00268067772490389322 *. year) (0.00162824170038242295 *. year)
        (-0.0000951592254519715870 *. year)
        (0.0000515138902046611451 *. solar_mass);
    |]
  in
  let px = ref 0.0 and py = ref 0.0 and pz = ref 0.0 in
  Array.iter
    (fun b ->
      px := !px +. (b.vx *. b.mass);
      py := !py +. (b.vy *. b.mass);
      pz := !pz +. (b.vz *. b.mass))
    bodies;
  bodies.(0).vx <- -. !px /. solar_mass;
  bodies.(0).vy <- -. !py /. solar_mass;
  bodies.(0).vz <- -. !pz /. solar_mass;
  bodies

let advance bodies dt =
  let n = Array.length bodies in
  for i = 0 to n - 1 do
    let bi = bodies.(i) in
    for j = i + 1 to n - 1 do
      let bj = bodies.(j) in
      let dx = bi.x -. bj.x in
      let dy = bi.y -. bj.y in
      let dz = bi.z -. bj.z in
      let d2 = (dx *. dx) +. (dy *. dy) +. (dz *. dz) in
      let mag = dt /. (d2 *. sqrt d2) in
      bi.vx <- bi.vx -. (dx *. bj.mass *. mag);
      bi.vy <- bi.vy -. (dy *. bj.mass *. mag);
      bi.vz <- bi.vz -. (dz *. bj.mass *. mag);
      bj.vx <- bj.vx +. (dx *. bi.mass *. mag);
      bj.vy <- bj.vy +. (dy *. bi.mass *. mag);
      bj.vz <- bj.vz +. (dz *. bi.mass *. mag)
    done
  done;
  Array.iter
    (fun b ->
      b.x <- b.x +. (dt *. b.vx);
      b.y <- b.y +. (dt *. b.vy);
      b.z <- b.z +. (dt *. b.vz))
    bodies

let energy bodies =
  let e = ref 0.0 in
  let n = Array.length bodies in
  for i = 0 to n - 1 do
    let b = bodies.(i) in
    e := !e +. (0.5 *. b.mass *. ((b.vx *. b.vx) +. (b.vy *. b.vy) +. (b.vz *. b.vz)));
    for j = i + 1 to n - 1 do
      let dx = b.x -. bodies.(j).x in
      let dy = b.y -. bodies.(j).y in
      let dz = b.z -. bodies.(j).z in
      e := !e -. (b.mass *. bodies.(j).mass /. sqrt ((dx *. dx) +. (dy *. dy) +. (dz *. dz)))
    done
  done;
  !e

let () =
  let n = size 1000 in
  let bodies = start () in
  let before = energy bodies in
  for _ = 1 to n do
    advance bodies 0.01
  done;
  let after = energy bodies in
  Printf.printf "%d %d %d\n" n (int_of_float (before *. 1e9)) (int_of_float (after *. 1e9))
