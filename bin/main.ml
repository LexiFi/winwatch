let handle_notif action filename =
  match action with
  | Winwatch.ADD -> Printf.printf "Added: %s%!" filename
  | Winwatch.REMOVE -> Printf.printf "Removed: %s%!" filename
  | Winwatch.MODIFY -> Printf.printf "Modified: %s%!" filename
  | Winwatch.RENAMED_OLD -> Printf.printf "Renamed from: %s%!" filename
  | Winwatch.RENAMED_NEW -> Printf.printf "          to: %s%!" filename

let rec watch_input state handle =
  match input_line stdin with
  | exception End_of_file
  | "exit" ->
    Winwatch.stop state;
    Thread.join handle;
    print_endline "File-watching has ended"
  | _ as path ->
    Winwatch.add state path;
    watch_input state handle

let file_watch paths =
  let state = Winwatch.create () in
  let info = Winwatch.{ watch_state = state; exclusions = [] } in
  Winwatch.set_exclusions info ["./.git"];
  List.iter (Winwatch.add state) paths;
  let handle = Thread.create (Winwatch.start info) handle_notif in
  print_endline "Type another path to watch or 'exit' to end directory watching";
  watch_input state handle

let () =
  match Array.to_list Sys.argv with
  | [] -> ()
  | _ :: t ->
    file_watch t;
    file_watch []
