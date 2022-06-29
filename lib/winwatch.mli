type action = ADD | REMOVE | MODIFY | RENAMED_OLD | RENAMED_NEW

type state

type t = 
{ watch_state : state;
  mutable exclusions : string list 
}

val create : unit -> state 

val add : state -> string -> unit 

val start : t -> (action -> string -> unit)  -> unit 

val stop : state -> unit 

val set_exclusions : t -> string list -> unit
