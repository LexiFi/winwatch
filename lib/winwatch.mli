type action = ADD | REMOVE | MODIFY | RENAMED_OLD | RENAMED_NEW

type state

type t 

val create : unit -> t 

val add : t -> string -> unit 

val start : t -> (action -> string -> unit)  -> unit 

val stop : t -> unit 

val set_exclusions : t -> string list -> unit
