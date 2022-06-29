type action = ADD | REMOVE | MODIFY | RENAMED_OLD | RENAMED_NEW

type state

type t = 
{ watch_state : state;
  mutable exclusions : string list 
}

external create: unit -> state = "winwatch_create"

external add: state -> string -> unit = "winwatch_add"

external start_watch: state -> (action -> string -> unit) -> unit = "winwatch_start"

external stop: state -> unit = "winwatch_stop"

let set_exclusions x paths  =
  x.exclusions <- paths

let rec should_exclude filename paths =
  (* Use List.exists (fun exc -> String.starts_with ... ) paths *)
  match paths with
  | [] -> false
  | h::t ->
    match String.starts_with ~prefix:h filename with
    | true -> true
    | false -> should_exclude filename t

let start t handler =
  start_watch t.watch_state (fun action filename ->
    if not (should_exclude filename t.exclusions) then
      handler action filename
    )
