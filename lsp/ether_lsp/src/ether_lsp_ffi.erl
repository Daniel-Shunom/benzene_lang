%% Low-level pieces the language server needs that Gleam cannot express on its
%% own: raw stdio in binary mode, and running the `ether` compiler as a child
%% process with a payload on its stdin.
-module(ether_lsp_ffi).

-export([configure_stdio/0, read_line/0, read_bytes/1, write_stdout/1,
         log/1, run_scan/3, find_compiler/0, halt/0]).

%% stdout is the LSP transport, so nothing else may write to it. The default
%% logger handler logs to standard_io, which would corrupt the message stream
%% the moment anything warned; it is removed rather than redirected.
configure_stdio() ->
    _ = logger:remove_handler(default),
    ok = io:setopts(standard_io, [binary]),
    nil.

%% Returns one line including its trailing newline, or `eof` at end of stream.
%% Header lines are short, so the cost of line-at-a-time reads is irrelevant.
read_line() ->
    case io:get_line(standard_io, "") of
        eof -> {error, <<"eof">>};
        {error, Reason} -> {error, list_to_binary(io_lib:format("~p", [Reason]))};
        Data when is_binary(Data) -> {ok, Data};
        Data when is_list(Data) -> {ok, list_to_binary(Data)}
    end.

%% Reads exactly N bytes. A short read means the client went away mid-message,
%% which is an error rather than a partial message worth parsing.
read_bytes(0) -> {ok, <<>>};
read_bytes(N) ->
    case io:get_chars(standard_io, "", N) of
        eof -> {error, <<"eof">>};
        {error, Reason} -> {error, list_to_binary(io_lib:format("~p", [Reason]))};
        Data ->
            Bin = iolist_to_binary(Data),
            case byte_size(Bin) of
                N -> {ok, Bin};
                _ -> {error, <<"short read">>}
            end
    end.

write_stdout(Data) ->
    ok = file:write(standard_io, Data),
    nil.

log(Message) ->
    _ = io:put_chars(standard_error, [Message, $\n]),
    nil.

%% Runs `Exe` with `Args`, writes `Payload` to its stdin, and collects stdout
%% until the child exits.
%%
%% `ether scan -stdin` reads a length-prefixed payload, so it never waits for
%% EOF on stdin -- which matters because an Erlang port cannot close the
%% child's stdin without also killing the process.
run_scan(Exe, Args, Payload) ->
    try
        Port = erlang:open_port(
                 {spawn_executable, binary_to_list(Exe)},
                 [{args, [binary_to_list(A) || A <- Args]},
                  binary, exit_status, use_stdio, hide]),
        case Payload of
            <<>> -> ok;
            _ -> true = erlang:port_command(Port, Payload)
        end,
        collect(Port, [])
    catch
        _:Reason ->
            {error, list_to_binary(io_lib:format("~p", [Reason]))}
    end.

collect(Port, Acc) ->
    receive
        {Port, {data, Data}} ->
            collect(Port, [Data | Acc]);
        {Port, {exit_status, 0}} ->
            {ok, iolist_to_binary(lists:reverse(Acc))};
        {Port, {exit_status, Code}} ->
            {error, list_to_binary("ether exited with status "
                                   ++ integer_to_list(Code))}
    after 15000 ->
        _ = (catch erlang:port_close(Port)),
        {error, <<"ether timed out">>}
    end.

%% Locates the `ether` binary. ETHER_BIN wins so a checkout can be pointed at
%% its own freshly built compiler without touching PATH.
find_compiler() ->
    case os:getenv("ETHER_BIN") of
        false -> search_path();
        "" -> search_path();
        Path ->
            case filelib:is_regular(Path) of
                true -> {ok, list_to_binary(Path)};
                false -> {error, list_to_binary(
                    "ETHER_BIN is set to " ++ Path ++ " but no such file exists")}
            end
    end.

search_path() ->
    case os:find_executable("ether") of
        false -> {error, <<"could not find `ether` on PATH; set ETHER_BIN to the compiler binary">>};
        Path -> {ok, list_to_binary(Path)}
    end.

halt() ->
    erlang:halt(0).
