let handle_notif action filename =
  match action with
  | Winwatch.ADD -> Printf.printf "Added: %s\n%!" filename
  | Winwatch.REMOVE -> Printf.printf "Removed: %s\n%!" filename
  | Winwatch.MODIFY -> Printf.printf "Modified: %s\n%!" filename
  | Winwatch.RENAMED_OLD -> Printf.printf "Renamed from: %s\n%!" filename
  | Winwatch.RENAMED_NEW -> Printf.printf "          to: %s\n%!" filename

let rec watch_input info handle =
  match input_line stdin with
  | exception End_of_file
  | "exit" ->
    Winwatch.stop info;
    Thread.join handle;
    print_endline "File-watching has ended"
  | _ as path ->
    Winwatch.add info path;
    watch_input info handle

let file_watch paths =
  let info = Winwatch.create () in
  Winwatch.set_exclusions info ["./.git"];
  List.iter (Winwatch.add info) paths;
  let handle = Thread.create (Winwatch.start info) handle_notif in
  print_endline "Type another path to watch or 'exit' to end directory watching";
  watch_input info handle

let () =
  match Array.to_list Sys.argv with
  | [] -> ()
  | _ :: t ->
    file_watch t;
    file_watch []
