"""Exercise an installed toolchain from outside its checkout, including relocation."""
import json
import os
from pathlib import Path
import queue
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"


def check(prefix):
    suffix = ".exe" if os.name == "nt" else ""
    bin_dir = prefix / "bin"
    compiler = bin_dir / ("ether" + suffix)
    lsp = bin_dir / ("ether-lsp" + suffix)
    mcp = bin_dir / ("ether-mcp" + suffix)
    for executable in [compiler, lsp, mcp]:
        assert executable.is_file(), executable
        version = subprocess.run([str(executable), "--version"], capture_output=True,
                                 text=True, check=True)
        assert "0.1.0" in version.stdout
    assert (prefix / "share" / "benzene" / "editors" / "nvim" / "lsp" / "benzene.lua").is_file()
    env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
    env.pop("ETHER_BIN", None)
    env.pop("BENZENE_ERL", None)

    with tempfile.TemporaryDirectory(prefix="installed-toolchain-", dir=BUILD) as directory:
        work = Path(directory).resolve()
        assert work.parent == BUILD.resolve(), "Unexpected cleanup directory"
        created = subprocess.run([str(compiler), "new", "sample"], cwd=work, env=env,
                                 capture_output=True, text=True, timeout=20)
        assert created.returncode == 0, created.stderr
        source_file = work / "sample" / "src" / "main.bz"
        scanned = subprocess.run([str(compiler), "scan", str(source_file)], cwd=work, env=env,
                                 capture_output=True, text=True, check=True)
        assert not json.loads(scanned.stdout)["diagnostics"]
        assert "ether-mcp" in (work / "sample" / "AGENTS.md").read_text()

        process = subprocess.Popen([str(lsp)], cwd=work, env=env, stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        replies = queue.Queue()

        def read():
            try:
                while True:
                    length = None
                    while True:
                        line = process.stdout.readline()
                        if not line:
                            return
                        if line.lower().startswith(b"content-length:"):
                            length = int(line.split(b":", 1)[1])
                        if line in (b"\r\n", b"\n"):
                            break
                    assert length is not None, "LSP launcher wrote non-protocol output"
                    replies.put(json.loads(process.stdout.read(length)))
            except Exception as error:
                replies.put(error)

        threading.Thread(target=read, daemon=True).start()

        def send(message):
            body = json.dumps(message).encode()
            process.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode() + body)
            process.stdin.flush()

        def receive(predicate):
            deadline = time.monotonic() + 20
            while True:
                message = replies.get(timeout=max(0.01, deadline - time.monotonic()))
                if isinstance(message, Exception):
                    raise message
                if predicate(message):
                    return message

        try:
            send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"capabilities": {}}})
            assert "hoverProvider" in receive(lambda m: m.get("id") == 1)["result"]["capabilities"]
            send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
            send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {
                "uri": source_file.as_uri(), "languageId": "benzene", "version": 1, "text": source_file.read_text(),
            }}})
            diagnostics = receive(lambda m: m.get("method") == "textDocument/publishDiagnostics")
            assert not diagnostics["params"]["diagnostics"], diagnostics
            send({"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover", "params": {
                "textDocument": {"uri": source_file.as_uri()}, "position": {"line": 0, "character": 5},
            }})
            assert "Nil" in receive(lambda m: m.get("id") == 2)["result"]["contents"]["value"]
            send({"jsonrpc": "2.0", "id": 3, "method": "shutdown", "params": {}})
            receive(lambda m: m.get("id") == 3)
            send({"jsonrpc": "2.0", "method": "exit", "params": {}})
            process.wait(timeout=5)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            process.stdin.close()
            process.stdout.close()
            process.stderr.close()

        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        server = subprocess.Popen([str(mcp)], cwd=work,
                                  env=dict(env, BENZENE_MCP_PORT=str(port)),
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        url = f"http://127.0.0.1:{port}/mcp"

        def rpc(method, params):
            body = json.dumps({"jsonrpc": "2.0", "id": "installed-test", "method": method, "params": params}).encode()
            request = urllib.request.Request(url, data=body, headers={
                "Content-Type": "application/json", "Accept": "application/json, text/event-stream",
                "MCP-Protocol-Version": "2025-11-25",
            })
            with urllib.request.urlopen(request, timeout=20) as response:
                return json.load(response)["result"]

        try:
            deadline = time.monotonic() + 20
            while True:
                assert server.poll() is None, "Installed MCP exited before listening"
                try:
                    rpc("initialize", {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "installed-test", "version": "1"}})
                    break
                except (urllib.error.URLError, ConnectionError):
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.1)
            assert {t["name"] for t in rpc("tools/list", {})["tools"]} == {"generate_program", "check_program"}
            generated = rpc("tools/call", {"name": "generate_program", "arguments": {"construct": {
                "kind": "Function", "identifier": "main", "return_type": {"kind": "NamedType", "name": "Nil"},
                "body": [{"kind": "NilValue"}],
            }}})
            assert not generated["isError"], generated
            assert generated["structuredContent"]["valid"] and generated["structuredContent"]["validated"]
            invalid = rpc("tools/call", {"name": "check_program", "arguments": {"source": 'func main() :> Int\n "bad"\nend'}})
            assert invalid["isError"] and not invalid["structuredContent"]["valid"]
        finally:
            server.terminate()
            server.wait(timeout=5)

        missing = subprocess.run([str(lsp)], cwd=work, env=dict(env, BENZENE_ERL=str(work / "no-runtime")),
                                 capture_output=True, text=True, timeout=10)
        assert missing.returncode != 0 and not missing.stdout and "Erlang/OTP" in missing.stderr
    print("PASS installed compiler, scaffold, LSP diagnostics/hover, MCP discovery/generation/type checking:", prefix)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python tests/integration/test_installation.py <installation-prefix>")
    prefix = Path(sys.argv[1]).resolve()
    check(prefix)
    with tempfile.TemporaryDirectory(prefix="relocated-toolchain-", dir=BUILD) as directory:
        workspace = Path(directory).resolve()
        assert workspace.parent == BUILD.resolve(), "Unexpected relocation cleanup directory"
        relocated = workspace / "Benzene with spaces"
        shutil.copytree(prefix, relocated)
        check(relocated)


if __name__ == "__main__":
    main()
