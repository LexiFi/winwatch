type action = ADD | REMOVE | MODIFY | RENAMED_OLD | RENAMED_NEW
type t

external create: unit -> t = "winwatch_create"

external add_path: t -> string -> unit = "winwatch_add_path"

external start_watch: t -> (action -> string -> unit) -> unit = "winwatch_start"

external stop_watching: t -> unit = "winwatch_stop_watching"

let exclusions = ref []

let set_exclusions paths =
  exclusions := paths

let rec should_exclude filename paths =
  (* Use List.exists (fun exc -> String.starts_with ... ) paths *)
  match paths with
  | [] -> false
  | h::t ->
    match String.starts_with ~prefix:h filename with
    | true -> true
    | false -> should_exclude filename t

let start state handler =
  start_watch state (fun action filename ->
      if not (should_exclude filename !exclusions) then
        handler action filename
    )
