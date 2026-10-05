"""A minimal LSP client for driving ether-lsp from the outside.

The Gleam unit tests cover the pure logic. These tests cover everything that
only shows up once a real server is talking to a real compiler over a real
pipe: framing, scheduling, subprocess failure, and timing.
"""

import glob
import json
import os
import subprocess
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SHIPMENT = os.path.join(ROOT, "lsp", "ether_lsp", "build", "erlang-shipment")
WINDOWS = os.name == "nt"


def compiler_path():
    """The `ether` binary built from this checkout."""
    name = "ether.exe" if WINDOWS else "ether"
    return os.path.join(ROOT, "bin", name)


def preflight():
    """Returns a complaint if the tests cannot run, or None if they can."""
    if not os.path.isdir(os.path.join(SHIPMENT, "ether_lsp", "ebin")):
        return "language server not built - run lsp/build.sh (or build.cmd)"
    if not os.path.isfile(compiler_path()):
        return "compiler not built - run build.bat (or cmake --build build)"
    return None


class Client:
    """Speaks LSP to a freshly launched server.

    Incoming messages are read on a background thread so that a test can send
    without first draining replies -- which is exactly the pattern needed to
    check that the server stays responsive under load.
    """

    def __init__(self, ether=None):
        pa = []
        for ebin in sorted(glob.glob(os.path.join(SHIPMENT, "*", "ebin"))):
            pa += ["-pa", ebin]

        env = dict(os.environ)
        env["ETHER_BIN"] = ether or compiler_path()

        self.process = subprocess.Popen(
            ["erl"] + pa + ["-eval", "ether_lsp@@main:run(ether_lsp)", "-noshell"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
        )
        self.next_id = 0
        self.inbox = []
        self.lock = threading.Lock()
        threading.Thread(target=self._pump, daemon=True).start()

    def _pump(self):
        while True:
            length = None
            while True:
                line = self.process.stdout.readline()
                if not line:
                    return
                line = line.decode(errors="replace").strip()
                if line == "":
                    break
                if line.lower().startswith("content-length:"):
                    length = int(line.split(":", 1)[1])
            if length is None:
                return
            body = self.process.stdout.read(length)
            with self.lock:
                self.inbox.append((time.monotonic(), json.loads(body)))

    # --- sending ---------------------------------------------------------

    def send(self, payload):
        body = json.dumps(payload).encode()
        self.process.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.process.stdin.flush()

    def notify(self, method, params):
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def request(self, method, params, timeout=30):
        self.next_id += 1
        mine = self.next_id
        self.send({"jsonrpc": "2.0", "id": mine, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self.lock:
                for _, message in self.inbox:
                    if message.get("id") == mine:
                        return message
            time.sleep(0.002)
        return None

    def result(self, method, params, timeout=30):
        reply = self.request(method, params, timeout)
        return reply.get("result") if reply else None

    # --- lifecycle -------------------------------------------------------

    def initialize(self, encodings=("utf-8", "utf-16")):
        reply = self.request("initialize", {
            "processId": None,
            "rootUri": None,
            "capabilities": {"general": {"positionEncodings": list(encodings)}},
        })
        self.notify("initialized", {})
        return reply

    def open(self, uri, text, wait=True):
        self.notify("textDocument/didOpen", {"textDocument": {
            "uri": uri, "languageId": "benzene", "version": 1, "text": text}})
        return self.await_diagnostics(uri) if wait else None

    def change(self, uri, text, version=2):
        self.notify("textDocument/didChange", {
            "textDocument": {"uri": uri, "version": version},
            "contentChanges": [{"text": text}]})

    def shutdown(self):
        self.request("shutdown", None)
        self.notify("exit", None)
        try:
            self.process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            self.process.kill()
            return None
        return self.process.returncode

    def stderr(self):
        return self.process.stderr.read().decode(errors="replace")

    # --- receiving -------------------------------------------------------

    def published(self, uri=None):
        with self.lock:
            found = [m for _, m in self.inbox
                     if m.get("method") == "textDocument/publishDiagnostics"]
        if uri is None:
            return found
        return [m for m in found if m["params"]["uri"] == uri]

    def await_diagnostics(self, uri, timeout=60):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            found = self.published(uri)
            if found:
                return found[-1]
            time.sleep(0.01)
        return None

    def drain(self):
        with self.lock:
            self.inbox = []

    @staticmethod
    def at(uri, line, character):
        return {
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character},
        }


class Report:
    """Collects pass/fail lines so a suite can print one summary."""

    def __init__(self, title):
        self.title = title
        self.failures = 0
        print(f"\n{title}")

    def section(self, name):
        print(f"  {name}")

    def check(self, label, ok, detail=""):
        mark = "PASS" if ok else "FAIL"
        suffix = f"   {detail}" if detail and not ok else ""
        print(f"    {mark}  {label}{suffix}")
        if not ok:
            self.failures += 1
        return ok

    def done(self):
        if self.failures:
            print(f"  {self.failures} failure(s) in {self.title}")
        return self.failures
