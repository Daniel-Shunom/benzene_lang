"""What the server does when things go wrong.

A broken compiler, a malformed request, and input the front-end was never
designed for are all ordinary events in an editor -- the compiler is routinely
rebuilt while a buffer is open, and most keystrokes land on something that does
not parse. None of them may take the server down.
"""

import os
import shutil
import subprocess
import tempfile

from client import Client, Report, compiler_path

URI = "file:///C:/work/resilience.bz"

DEGENERATE = [
    ("empty file", ""),
    ("whitespace only", "   \n\n\t\n"),
    ("no trailing newline", "const x = 1"),
    ("crlf line endings", "const x = 1\r\nconst y = 2\r\n"),
    ("unterminated string", 'const s = "oops\n'),
    ("unterminated block", "func f()\n  let x = 1\n"),
    ("stray delimiters", "end\nend\n}\n)\n"),
    ("only a comment", "Cmt nothing here\n"),
    ("very long line", "const x = " + " + ".join(["1"] * 2000) + "\n"),
    ("deeply nested", "func f()\n" + "{\n" * 50 + "1\n" + "}\n" * 50 + "end\n"),
    ("multi-byte text", 'const s = "héllo wörld ✓ 日本語"\n'),
]

POSITION_REQUESTS = [
    "textDocument/hover",
    "textDocument/definition",
    "textDocument/completion",
    "textDocument/signatureHelp",
    "textDocument/documentHighlight",
    "textDocument/prepareRename",
]


def build_fake(directory, name, body):
    """Compiles a stand-in `ether`. Returns None when no C compiler is around."""
    compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        return None

    source = os.path.join(directory, name + ".c")
    binary = os.path.join(directory, name + (".exe" if os.name == "nt" else ""))
    with open(source, "w", encoding="utf-8") as handle:
        handle.write(body)

    built = subprocess.run([compiler, source, "-o", binary],
                           capture_output=True)
    return binary if built.returncode == 0 else None


def broken_compiler_session(report, ether, label):
    client = Client(ether=ether)
    try:
        started = client.initialize()
        report.check(f"{label}: the server still starts",
                     started is not None and "result" in started)

        published = client.open(URI, "const x = 1\n")
        diagnostics = published["params"]["diagnostics"] if published else []
        report.check(f"{label}: the failure is reported in the editor",
                     len(diagnostics) == 1
                     and "could not run" in diagnostics[0]["message"],
                     str(diagnostics))

        reply = client.request("textDocument/hover", Client.at(URI, 0, 6))
        report.check(f"{label}: requests are still answered",
                     reply is not None and "result" in reply)

        # A failed check must not be retried on every request, or a broken
        # toolchain means a doomed process per keystroke.
        client.drain()
        for _ in range(5):
            client.request("textDocument/hover", Client.at(URI, 0, 6))
        report.check(f"{label}: a failed check is not repeated per request",
                     len(client.published(URI)) == 0,
                     f"{len(client.published(URI))} re-runs")

        client.shutdown()
    finally:
        if client.process.poll() is None:
            client.process.kill()


def run():
    report = Report("resilience")

    report.section("degenerate input")
    client = Client()
    try:
        client.initialize()
        for name, text in DEGENERATE:
            uri = "file:///C:/work/%s.bz" % name.replace(" ", "_")
            published = client.open(uri, text)
            answered = all(
                client.request(method, Client.at(uri, 0, 0)) is not None
                for method in ("textDocument/hover",
                               "textDocument/semanticTokens/full")
            ) if published else False
            report.check(name, published is not None and answered)

        report.section("requests for documents the server does not have")
        ghost = "file:///C:/work/never-opened.bz"
        for method in POSITION_REQUESTS:
            reply = client.request(method, Client.at(ghost, 5, 5))
            report.check(method, reply is not None and "result" in reply)
        reply = client.request("textDocument/rename",
                               dict(Client.at(ghost, 5, 5), newName="ok"))
        report.check("rename errors rather than hanging",
                     reply is not None and "error" in reply)

        report.section("closing")
        closing = "file:///C:/work/closing.bz"
        client.open(closing, "const x = 1\nconst x = 2\n")
        client.drain()
        client.notify("textDocument/didClose", {"textDocument": {"uri": closing}})
        cleared = client.await_diagnostics(closing, timeout=10)
        report.check("closing clears its diagnostics",
                     cleared and cleared["params"]["diagnostics"] == [],
                     str(cleared))
        report.check("requests after close still reply",
                     client.request("textDocument/hover",
                                    Client.at(closing, 0, 6)) is not None)

        report.section("malformed traffic")
        client.send({"jsonrpc": "2.0", "id": 9001, "method": "textDocument/hover",
                     "params": {"textDocument": {"uri": 12345},
                                "position": "nonsense"}})
        report.check("a malformed request gets a reply",
                     client.request("textDocument/hover",
                                    Client.at(URI, 0, 0)) is not None)

        unknown = client.request("textDocument/somethingElse", {})
        report.check("an unknown method errors rather than hanging",
                     unknown is not None and "error" in unknown)

        client.send({"jsonrpc": "2.0", "method": "$/cancelRequest",
                     "params": {"id": 1}})
        report.check("cancellation is tolerated",
                     client.request("textDocument/hover",
                                    Client.at(URI, 0, 0)) is not None)

        report.check("exits cleanly", client.shutdown() == 0)
    finally:
        if client.process.poll() is None:
            client.process.kill()

    report.section("a compiler that does not work")
    with tempfile.TemporaryDirectory() as workspace:
        failing = build_fake(workspace, "failing",
                             '#include <stdio.h>\n'
                             'int main(void){fprintf(stderr,"boom\\n");return 3;}\n')
        garbage = build_fake(workspace, "garbage",
                             '#include <stdio.h>\n'
                             'int main(void){printf("not json\\n");return 0;}\n')

        if failing is None:
            report.check("skipped - no C compiler to build a stand-in with", True)
        else:
            broken_compiler_session(report, failing, "exits non-zero")
            broken_compiler_session(report, garbage, "prints garbage")

        missing = os.path.join(workspace, "does-not-exist")
        client = Client(ether=missing)
        try:
            # The server has nothing to serve, so it is expected to stop rather
            # than sit there answering nothing.
            client.process.wait(timeout=20)
            report.check("a missing compiler stops the server at startup", True)
        except subprocess.TimeoutExpired:
            report.check("a missing compiler stops the server at startup", False,
                         "still running")
            client.process.kill()

    return report.done()


if __name__ == "__main__":
    raise SystemExit(1 if run() else 0)
