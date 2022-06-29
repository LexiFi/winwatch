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

let normalize path = 
  String.concat "/" [Filename.dirname path; Filename.basename path]

let set_exclusions t paths  =
  t.exclusions <- (List.map normalize paths)

let start t handler =
  aux_start t.watch_state (fun action filename dir_path ->
    let file_path = normalize ((dir_path ^ "/") ^ filename) in
    if not (should_exclude file_path t.exclusions) then
      handler action file_path
    )

let add t path = 
  aux_add t.watch_state path

let stop t =
  aux_stop t.watch_state
