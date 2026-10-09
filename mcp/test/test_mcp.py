"""Exercise the actual HTTP MCP server and retain a generated program/report."""
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import urllib.error
import urllib.request

from programs.file_http import program

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "mcp" / "ether_mcp"
OUTPUT = ROOT / "mcp" / "examples"


def main():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    env = dict(os.environ, BENZENE_MCP_PORT=str(port), ETHER_BIN=str(ROOT / "bin" / "ether.exe"))
    subprocess.run(["gleam", "build"], cwd=PROJECT, env=env, check=True)
    # Start erl directly: killing a gleam launcher can leave the VM behind.
    libs = PROJECT / "build" / "dev" / "erlang"
    command = ["erl", "-noshell", "-pa"] + [str(p) for p in libs.glob("*/ebin")]
    command += ["-eval", "'mcp@server':main()."]
    server = subprocess.Popen(command, cwd=PROJECT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    url = f"http://127.0.0.1:{port}/mcp"
    count = 0

    def send(body, headers=None, method="POST", endpoint=url):
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(endpoint, data=data, method=method, headers={
            "Content-Type": "application/json", "Accept": "application/json, text/event-stream",
            "MCP-Protocol-Version": "2025-11-25", **(headers or {}),
        })
        try:
            with urllib.request.urlopen(req, timeout=25) as response:
                payload = response.read()
                return response.status, json.loads(payload) if payload else None
        except urllib.error.HTTPError as error:
            payload = error.read()
            if payload and "application/json" in error.headers.get("Content-Type", ""):
                return error.code, json.loads(payload)
            return error.code, payload.decode() if payload else None

    def rpc(method, params=None):
        nonlocal count
        count += 1
        status, reply = send({"jsonrpc": "2.0", "id": str(count), "method": method, "params": params or {}})
        assert status == 200 and reply["id"] == str(count), (status, reply)
        return reply

    def tool(name, arguments):
        reply = rpc("tools/call", {"name": name, "arguments": arguments})
        assert "error" not in reply, reply
        result = reply["result"]
        assert json.loads(result["content"][0]["text"]) == result["structuredContent"]
        return result

    try:
        deadline = time.monotonic() + 20
        while True:
            assert server.poll() is None, "MCP server exited before listening"
            try:
                rpc("ping")
                break
            except (urllib.error.URLError, ConnectionError):
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.1)
        initialized = rpc("initialize", {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "integration-test", "version": "1"}})
        assert initialized["result"]["protocolVersion"] == "2025-11-25"
        assert send({"jsonrpc": "2.0", "method": "notifications/initialized"}) == (202, None)
        tools = rpc("tools/list")["result"]["tools"]
        assert {t["name"] for t in tools} == {"generate_program", "check_program"}
        kinds = {item["properties"]["kind"]["const"] for item in tools[0]["inputSchema"]["$defs"]["Construct"]["oneOf"]}
        assert {"Lambda", "CaseExpr", "TypeDeclaration"} <= kinds
        generated = tool("generate_program", {"construct": program(), "path": "file_http.bz"})
        assert not generated["isError"], generated
        data = generated["structuredContent"]
        assert data["validated"] and data["valid"] and not data["analysis"]["diagnostics"]
        index = data["analysis"]["index"]
        assert any(entry["name"] == "app" and entry["isDefinition"] for entry in index)
        OUTPUT.mkdir(exist_ok=True)
        (OUTPUT / "file_http.bz").write_text(data["source"] + "\n", encoding="utf-8")
        (OUTPUT / "file_http.request.json").write_text(json.dumps({"jsonrpc": "2.0", "id": "example", "method": "tools/call", "params": {"name": "generate_program", "arguments": {"construct": program(), "path": "file_http.bz"}}}, indent=2) + "\n", encoding="utf-8")
        checked = tool("check_program", {"source": data["source"], "path": "file_http.bz"})
        assert not checked["isError"] and checked["structuredContent"]["valid"]
        undefined = tool("check_program", {"source": "func main()\n missing()\nend"})
        assert undefined["isError"]
        assert any("not defined" in d["message"] for d in undefined["structuredContent"]["analysis"]["diagnostics"])
        mismatch = tool("check_program", {"source": "func main() :> Int\n \"wrong\"\nend"})
        assert mismatch["isError"] and not mismatch["structuredContent"]["valid"]
        invalid = tool("generate_program", {"construct": {"kind": "Module", "data": [{"kind": "Let", "identifier": "x", "value": {"kind": "Integer", "value": 1}}]}})
        assert invalid["isError"] and invalid["structuredContent"]["stage"] == "formation"
        malformed = tool("generate_program", {"construct": {"kind": "NotAConstruct"}})
        assert malformed["isError"] and malformed["structuredContent"]["stage"] == "decoding"
        fragment = tool("generate_program", {"construct": {"kind": "Integer", "value": 42}, "validate": False})
        assert not fragment["isError"] and not fragment["structuredContent"]["validated"]
        assert rpc("unknown")["error"]["code"] == -32601
        assert send(None, method="GET")[0] == 405
        assert send({"jsonrpc": "2.0", "id": 1, "method": "ping"}, {"Origin": "https://malicious.example"})[0] == 403
        assert send({"jsonrpc": "2.0", "id": 1, "method": "ping"}, {"MCP-Protocol-Version": "unsupported"})[0] == 400
        assert send(None, method="GET", endpoint=url + "/missing")[0] == 404
        print("PASS HTTP initialization, discovery, generation, compilation, diagnostics, formation and transport checks")
        print("Generated and type-checked:", OUTPUT / "file_http.bz")
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()


if __name__ == "__main__":
    main()
