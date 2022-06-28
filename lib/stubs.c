#include <stdio.h>
#include <caml/mlvalues.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/osdeps.h>
#include <caml/memory.h>
#include <caml/threads.h>
#include <caml/unixsupport.h>
#include <caml/custom.h>
#include <windows.h>
#include <wchar.h>

#define BUFF_SIZE 1024

struct global_state
{
    struct path_node *head;
    HANDLE completion_port;
    BOOL file_watching_stopped;
};

struct path_node
{
    char *buffer;
    const WCHAR *path;
    HANDLE handle;
    OVERLAPPED overlapped;
    struct path_node *next;
};

enum request_type
{
    FileChange,
    Stop,
    AddPath
};

struct request
{
    enum request_type type;
    union
    {
        struct path_node* file_change;
        const WCHAR* path;
    };
};

void finalization(value v_block)
{
    struct global_state *state = *(struct global_state**) Data_custom_val(v_block);

    if (state->file_watching_stopped)
    {
        caml_stat_free(state);
    }
}

static struct custom_operations global_state_ops =
{
    "winwatch.state",           finalization,
    custom_compare_default,     custom_hash_default,
    custom_serialize_default,   custom_deserialize_default,
    custom_compare_ext_default, custom_fixed_length_default
};

static LPTSTR print_error(HANDLE handle)
{
    LPTSTR lpMsgBuf;
    DWORD dw;

    dw = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf,
        0, NULL );

    CloseHandle(handle);

    return lpMsgBuf;
}

value winwatch_create(value v_unit)
{
    CAMLparam1(v_unit);
    value v_state;
    struct global_state *state = NULL;

    state = caml_stat_alloc(sizeof(struct global_state));
    state->head = NULL;
    state->file_watching_stopped = FALSE;
    state->completion_port = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,
        NULL,
        0,
        1);

    v_state = caml_alloc_custom(&global_state_ops, sizeof(struct global_state *), 0, 1);
    *((struct global_state **)Data_custom_val(v_state)) = state;

    CAMLreturn(v_state);
}

value winwatch_add_path(value v_state, value v_path)
{
    CAMLparam2(v_state, v_path);
    WCHAR *path = NULL;
    struct global_state *state = NULL;
    struct request *add_request = NULL;

    path = caml_stat_strdup_to_utf16(String_val(v_path));

    state = *(struct global_state**)(Data_custom_val(v_state));
    add_request = malloc(sizeof(struct request));

    add_request->type = AddPath;
    add_request->path = path;


    BOOL add_packet_posted = PostQueuedCompletionStatus(
        state->completion_port,
        0,
        (ULONG_PTR)add_request,
        NULL);

    if (add_packet_posted == FALSE)
    {
        caml_failwith("PostQueuedCompletionStatus failed.");
    }
    CAMLreturn(Val_unit);
}

value winwatch_start(value v_state, value v_func)
{
    CAMLparam2(v_state, v_func);
    CAMLlocal2(v_file_path, v_action);

    struct global_state* state = NULL;
    DWORD num_bytes = 0;
    OVERLAPPED *overlapped = NULL;
    char* path;
    BOOL stop = FALSE;
    struct request* notif;
    ULONG_PTR completion_key;
    struct path_node* tmp = NULL;

    state = *(struct global_state**)(Data_custom_val(v_state));

    while (!stop)
    {
        caml_release_runtime_system();

        BOOL got_packet = GetQueuedCompletionStatus(
            state->completion_port,
            &num_bytes,
            &completion_key,
            &overlapped,
            INFINITE);

        caml_acquire_runtime_system();

        notif = (struct request*)completion_key;

        if (got_packet == FALSE)
        {
            LPTSTR error_msg = print_error(state->completion_port);
            uerror("GetQCompletionStatus failed", caml_copy_string(error_msg));
        }

        switch (notif->type)
        {
            case Stop:
            {
                stop = TRUE;
                break;
            }

            case FileChange:
            {
                struct path_node *data = NULL;
                FILE_NOTIFY_INFORMATION *event = NULL;
                DWORD name_len;
                wchar_t *file_path;

                data = notif->file_change;
                event = (FILE_NOTIFY_INFORMATION*)data->buffer;

                /*Iterates over file change notifications*/
                for (;;)
                {
                    name_len = event->FileNameLength / sizeof(wchar_t);

                    switch (event->Action)
                    {
                        case FILE_ACTION_ADDED:
                            v_action = Val_int(0);
                            break;
                        case FILE_ACTION_REMOVED:
                            v_action = Val_int(1);
                            break;
                        case FILE_ACTION_MODIFIED:
                            v_action = Val_int(2);
                            break;
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            v_action = Val_int(3);
                            break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            v_action = Val_int(4);
                            break;
                    }

                    DWORD path_len = wcslen(data->path);

                    file_path = malloc(sizeof(WCHAR) * (name_len + path_len + 2));
                    memcpy(file_path, data->path, sizeof(WCHAR) * path_len);
                    file_path[path_len] = L'/';
                    memcpy(file_path + path_len + 1, event->FileName, sizeof(WCHAR) * name_len);
                    file_path[path_len + name_len + 1] = 0;
                    v_file_path = caml_copy_string_of_os(file_path);
                    free(file_path);

                    caml_callback2(v_func, v_action, v_file_path);

                    if (event->NextEntryOffset)
                    {
                        *((char**)&event) += event->NextEntryOffset;
                    }
                    else
                    {
                        break;
                    }
                }

                memset(&(notif->file_change->overlapped), 0, sizeof(OVERLAPPED));

                BOOL watch_path = ReadDirectoryChangesW(
                    notif->file_change->handle, notif->file_change->buffer, BUFF_SIZE, TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME  |
                    FILE_NOTIFY_CHANGE_DIR_NAME   |
                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                    NULL, &(notif->file_change->overlapped), NULL);

                if (watch_path == FALSE)
                {
                    LPTSTR error_msg = print_error(notif->file_change->handle);
                    uerror("ReadDirectoryChangesW failed", caml_copy_string(error_msg));
                }
            } break;

            case AddPath:
            {
                struct path_node *new_node = NULL;
                char *change_buf = NULL;
                struct request* change_request = NULL;

                new_node = malloc(sizeof(struct path_node));
                change_buf = malloc(BUFF_SIZE * sizeof(char));
                new_node->buffer = change_buf;
                new_node->path = notif->path;

                new_node->handle = CreateFileW(new_node->path,
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                    NULL);

                if (new_node->handle == INVALID_HANDLE_VALUE)
                {
                    caml_failwith("Directory cannot be found.");
                }

                memset(&(new_node->overlapped), 0, sizeof(OVERLAPPED));

                change_request = malloc(sizeof(struct request));
                change_request->type = FileChange;
                change_request->file_change = new_node;

                state->completion_port = CreateIoCompletionPort(
                    new_node->handle,
                    state->completion_port,
                    (ULONG_PTR)change_request,
                    1);

                if (state->completion_port == NULL)
                {
                    caml_failwith("File could not be added to completion port.");
                }

                BOOL watch_path = ReadDirectoryChangesW(
                    new_node->handle, new_node->buffer, BUFF_SIZE, TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME  |
                    FILE_NOTIFY_CHANGE_DIR_NAME   |
                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                    NULL, &(new_node->overlapped), NULL);

                if (watch_path == FALSE)
                {
                    LPTSTR error_msg = print_error(new_node->handle);
                    uerror("ReadDirectoryChangesW failed", caml_copy_string(error_msg));
                }

                wprintf(L"Watching %ls\n", new_node->path);
                fflush(stdout);

                new_node->next = state->head;
                state->head = new_node;

            } break;

        free(notif);

        }
    }

    while (state->head != NULL)
    {
       tmp = state->head;

       BOOL stopped_watching = CancelIo(tmp->handle);
       if (stopped_watching == FALSE)
       {
            caml_failwith("CancelIO failed.");
       }

       CloseHandle(tmp->handle);
       free(tmp->buffer);
       free(tmp);
       state->head = (state->head)->next;
    }
    tmp = state->head;
    free(tmp);

    state->file_watching_stopped = TRUE;

    CAMLreturn(Val_unit);
}

value winwatch_stop_watching(value v_state)
{
    CAMLparam1(v_state);
    struct global_state* state = NULL;
    struct request* stopReq = NULL;
    OVERLAPPED *overlapped = NULL;

    state = *(struct global_state**)Data_custom_val(v_state);
    stopReq = malloc(sizeof(struct request));
    stopReq->type = Stop;

    BOOL stop_packet_posted = PostQueuedCompletionStatus(
        state->completion_port,
        0,
        (ULONG_PTR)stopReq,
        overlapped);

    if (stop_packet_posted == FALSE)
    {
        LPTSTR error_msg = print_error(state->completion_port);
        uerror("PostQueuedCompletionStatus", caml_copy_string(error_msg));
    }

    CAMLreturn(Val_unit);
}
