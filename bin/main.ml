let handle_notif action filename = 
    match action with
    | Winwatch.ADD -> Printf.printf "Added: %s\n%!" filename
    | Winwatch.REMOVE -> Printf.printf "Removed: %s\n%!" filename
    | Winwatch.MODIFY -> Printf.printf "Modified: %s\n%!" filename
    | Winwatch.RENAMED_OLD -> Printf.printf "Renamed from: %s\n%!" filename
    | Winwatch.RENAMED_NEW -> Printf.printf "          to: %s\n%!" filename

let start_thread state =
  Winwatch.start state handle_notif

let rec watch_input state handle =
  match input_line stdin with
  | "exit" ->
    Winwatch.stop_watching state;
    Thread.join handle;
    Printf.printf "File-watching has ended\n%!"
  | _ as path -> 
    Winwatch.add_path state path;
    watch_input state handle

let file_watch paths =
  let state = Winwatch.create () in
  List.iter (Winwatch.add_path state) paths;
  let handle = Thread.create start_thread state in
  Printf.printf "Type another path to watch or 'exit' to end directory watching\n%!";
  watch_input state handle  

let () =
  Winwatch.set_exclusions ["../../testdir1"]; 
  match Array.to_list Sys.argv with
  | [] -> ()
  | _::t -> file_watch t;
  file_watch []
