-module(ether_mcp_ffi).
-export([scan/2, port/0]).

port() ->
    case os:getenv("BENZENE_MCP_PORT") of
        false -> 4000;
        Value -> list_to_integer(Value)
    end.

%% No shell, no source-file writes: use the compiler's length-prefixed stdin.
scan(Path, Source) ->
    case compiler() of
        {error, _} = Error -> Error;
        {ok, Exe} ->
            Previous = process_flag(trap_exit, true),
            try
                Port = open_port({spawn_executable, Exe},
                    [{args, ["scan", binary_to_list(Path), "-stdin"]},
                     binary, exit_status, use_stdio, hide]),
                try
                    true = port_command(Port,
                        [integer_to_binary(byte_size(Source)), <<"\n">>, Source]),
                    collect(Port, [], erlang:monotonic_time(millisecond) + 15000)
                after
                    catch port_close(Port),
                    receive {'EXIT', Port, _} -> ok after 0 -> ok end
                end
            catch
                _:Reason -> {error, iolist_to_binary(io_lib:format("Compiler failed: ~p", [Reason]))}
            after
                process_flag(trap_exit, Previous)
            end
    end.

collect(Port, Acc, Deadline) ->
    Remaining = max(0, Deadline - erlang:monotonic_time(millisecond)),
    receive
        {Port, {data, Data}} -> collect(Port, [Data | Acc], Deadline);
        {Port, {exit_status, 0}} -> {ok, iolist_to_binary(lists:reverse(Acc))};
        {Port, {exit_status, Code}} ->
            {error, iolist_to_binary(io_lib:format("Compiler exited with status ~p", [Code]))};
        {'EXIT', Port, normal} -> collect(Port, Acc, Deadline);
        {'EXIT', Port, Reason} ->
            {error, iolist_to_binary(io_lib:format("Compiler process failed: ~p", [Reason]))}
    after Remaining -> {error, <<"Compiler timed out after 15 seconds">>}
    end.

compiler() ->
    case os:getenv("ETHER_BIN") of
        false -> find_compiler();
        "" -> find_compiler();
        Path -> executable(Path)
    end.

find_compiler() ->
    Extension = case os:type() of {win32, _} -> ".exe"; _ -> "" end,
    Local = filename:absname("../../bin/ether" ++ Extension),
    case filelib:is_regular(Local) of
        true -> {ok, Local};
        false -> case os:find_executable("ether") of
            false -> {error, <<"Compiler not found; set ETHER_BIN to the ether executable">>};
            Path -> executable(Path)
        end
    end.

executable(Path) ->
    case filelib:is_regular(Path) of
        true -> {ok, filename:absname(Path)};
        false -> {error, <<"ETHER_BIN does not point to a compiler executable">>}
    end.
