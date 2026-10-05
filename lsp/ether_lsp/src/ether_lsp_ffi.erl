%% Low-level pieces the language server needs that Gleam cannot express on its
%% own: raw stdio in binary mode, running the `ether` compiler as a child
%% process, a reader process so reading never blocks on analysis, and a crash
%% guard so one bad message cannot take the server down.
-module(ether_lsp_ffi).

-export([configure_stdio/0, read_line/0, read_bytes/1, write_stdout/1,
         log/1, run_scan/3, find_compiler/0, halt/0,
         spawn_reader/1, self_pid/0, send_frame/2, send_closed/2,
         receive_frame/1, guard/2, guard_with/2, monotonic_ms/0]).

%% stdout is the LSP transport, so nothing else may write to it. The default
%% logger handler logs to standard_io, which would corrupt the message stream
%% the moment anything warned; it is removed rather than redirected.
configure_stdio() ->
    _ = logger:remove_handler(default),
    ok = io:setopts(standard_io, [binary]),
    nil.

%% Returns one line including its trailing newline, or an error at end of
%% stream. Header lines are short, so line-at-a-time reads cost nothing.
read_line() ->
    case io:get_line(standard_io, "") of
        eof -> {error, <<"eof">>};
        {error, Reason} -> {error, format(Reason)};
        Data when is_binary(Data) -> {ok, Data};
        Data when is_list(Data) -> {ok, list_to_binary(Data)}
    end.

%% Reads exactly N bytes. A short read means the client went away mid-message,
%% which is an error rather than a partial message worth parsing.
read_bytes(0) -> {ok, <<>>};
read_bytes(N) ->
    case io:get_chars(standard_io, "", N) of
        eof -> {error, <<"eof">>};
        {error, Reason} -> {error, format(Reason)};
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

monotonic_ms() ->
    erlang:monotonic_time(millisecond).

%% --- reader process ---------------------------------------------------------
%%
%% Reading runs in its own process so that analysis -- which shells out to the
%% compiler and can take tens of milliseconds -- never stops the server from
%% draining its input. That is what lets a burst of keystrokes collapse into a
%% single check instead of a queue of stale ones.

%% The reader is linked, so nothing can leave the server running with no input.
%% It is also wrapped: an unexpected crash in here would otherwise take the
%% whole server down through that link, when reporting the loss of the stream
%% is both truer and recoverable.
spawn_reader(Fun) ->
    _ = spawn_link(fun() ->
        try
            Fun()
        catch
            Class:Reason:Stack ->
                log(format_crash(Class, Reason, Stack))
        end
    end),
    nil.

self_pid() ->
    self().

send_frame(Pid, Message) ->
    Pid ! {lsp_frame, Message},
    nil.

send_closed(Pid, Reason) ->
    Pid ! {lsp_closed, Reason},
    nil.

%% Blocks for at most `Timeout` milliseconds; a negative timeout waits forever.
%% The `idle` return is what drives debouncing: it means the client has gone
%% quiet and queued work can be flushed.
receive_frame(Timeout) ->
    After = case Timeout < 0 of
                true -> infinity;
                false -> Timeout
            end,
    receive
        {lsp_frame, Message} -> {frame, Message};
        {lsp_closed, Reason} -> {closed, Reason}
    after After ->
        idle
    end.

%% Runs `Fun`, returning `Fallback` if it throws. A malformed request or an
%% unexpected shape in a reply should cost one message, not the session.
guard(Fun, Fallback) ->
    try
        Fun()
    catch
        Class:Reason:Stack ->
            log(format_crash(Class, Reason, Stack)),
            Fallback
    end.

%% Like `guard/2`, but the recovery value is computed only on failure -- which
%% is what lets a failed request reply with an error instead of silently
%% leaving the client waiting.
guard_with(Fun, Recover) ->
    try
        Fun()
    catch
        Class:Reason:Stack ->
            log(format_crash(Class, Reason, Stack)),
            Recover()
    end.

%% `epipe` is what a port reports when the child is gone before its input is
%% written -- almost always the wrong binary, or one that exits immediately --
%% so it is worth saying so rather than passing the atom through.
explain(epipe) ->
    <<"the compiler exited before reading its input; check that ETHER_BIN "
      "points at a working `ether` binary">>;
explain(Reason) ->
    iolist_to_binary(io_lib:format("the compiler could not be run (~p)", [Reason])).

format_crash(Class, Reason, Stack) ->
    iolist_to_binary(
      io_lib:format("ether-lsp: recovered from ~p:~p~n~p", [Class, Reason, Stack])).

format(Term) ->
    iolist_to_binary(io_lib:format("~p", [Term])).

%% --- compiler ---------------------------------------------------------------

%% Runs `Exe` with `Args`, writes `Payload` to its stdin, and collects stdout
%% until the child exits.
%%
%% `ether scan -stdin` reads a length-prefixed payload, so it never waits for
%% EOF on stdin -- which matters because an Erlang port cannot close the
%% child's stdin without also killing the process.
%%
%% The port is opened inside a monitored worker rather than here. A port owner
%% is linked to its port, so a child that dies mid-write (a half-built binary,
%% a crash) delivers an asynchronous `epipe` exit signal that no try/catch in
%% the owner can intercept -- it would take the whole server down. Isolating the
%% port means that failure arrives as an ordinary `DOWN` message instead.
run_scan(Exe, Args, Payload) ->
    Parent = self(),
    {Worker, Ref} = spawn_monitor(fun() ->
        Parent ! {scan_result, self(), do_run_scan(Exe, Args, Payload)}
    end),
    receive
        {scan_result, Worker, Result} ->
            erlang:demonitor(Ref, [flush]),
            Result;
        {'DOWN', Ref, process, Worker, Reason} ->
            {error, explain(Reason)}
    after 20000 ->
        erlang:demonitor(Ref, [flush]),
        exit(Worker, kill),
        {error, <<"ether timed out">>}
    end.

do_run_scan(Exe, Args, Payload) ->
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
            {error, format(Reason)}
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
        %% The child is wedged. Closing the port kills it, which is the only
        %% way to stop a run that will never produce output.
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
