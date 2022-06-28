type action = ADD | REMOVE | MODIFY | RENAMED_OLD | RENAMED_NEW

type t

val create : unit -> t 

val add_path : t -> string -> unit 

val start : t -> (action -> string -> unit)  -> unit 

val stop_watching : t -> unit 

val set_exclusions : string list -> unit
