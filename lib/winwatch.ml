type action = ADD | REMOVE | MODIFY | RENAMED_OLD | RENAMED_NEW

type state

type t = 
{ watch_state : state;
  mutable exclusions : string list 
}

external aux_create: unit -> state = "winwatch_create"

external aux_add: state -> string -> unit = "winwatch_add"

external aux_start: state -> (action -> string -> string -> unit) -> unit = "winwatch_start"

external aux_stop: state -> unit = "winwatch_stop"

let rec should_exclude filename paths =
  (* Use List.exists (fun exc -> String.starts_with ... ) paths *)
  match paths with
  | [] -> false
  | h::t ->
    match String.starts_with ~prefix:h filename with
    | true -> true
    | false -> should_exclude filename t

let create () = 
  let t = { watch_state = aux_create (); exclusions = [] } in
  t

let set_exclusions t paths  =
  t.exclusions <- paths

let start t handler =
  aux_start t.watch_state (fun action filename dir_path ->
    if not (should_exclude ((dir_path ^ "/") ^ filename) t.exclusions) then
      handler action ((dir_path ^ "/") ^ filename) 
    )

let add t path = 
  aux_add t.watch_state path

let stop t =
  aux_stop t.watch_state
