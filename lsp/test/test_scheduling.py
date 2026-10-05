"""Debouncing, coalescing, and staying responsive while the compiler works.

These are the properties that make the server usable rather than merely
correct, and none of them are visible from a single request.
"""

import time

from client import Client, Report

URI = "file:///C:/work/typing.bz"

SOURCE = """func identity(x: Int) :> Int
  x
end

func main()
  let result = identity(42)
  result
end
"""

# Enough functions that one check takes the compiler several seconds. The
# front-end scales roughly quadratically, so this is deliberately large.
SLOW = "\n".join(
    "func fn_%d(a_%d: Int) :> Int\n  let v_%d = a_%d\n  v_%d\nend\n" % (i, i, i, i, i)
    for i in range(1500)
)


def run():
    report = Report("scheduling")
    client = Client()
    try:
        client.initialize()
        client.open(URI, SOURCE)
        time.sleep(0.5)
        client.drain()

        report.section("debouncing")
        edits = 40
        started = time.monotonic()
        for i in range(edits):
            client.change(URI, SOURCE + "\nlet pad_%d = %d\n" % (i, i), i + 2)
        accepted = time.monotonic() - started
        time.sleep(2.0)

        runs = len(client.published(URI))
        report.check(f"{edits} rapid edits collapse to {runs} compiler run(s)",
                     runs <= 4, f"got {runs}")
        report.check(f"edits accepted without blocking ({accepted * 1000:.0f}ms)",
                     accepted < 1.5, f"{accepted:.2f}s")

        report.section("freshness")
        client.drain()
        client.change(URI, "const a = 1\nconst a = 2\n", 500)
        time.sleep(1.5)
        published = client.published(URI)
        last = published[-1]["params"]["diagnostics"] if published else []
        report.check("the final edit is the one reported on",
                     len(last) == 1 and "Duplicate" in last[0]["message"], str(last))

        report.section("requests during a burst")
        client.drain()
        for i in range(20):
            client.change(URI, SOURCE, 600 + i)

        started = time.monotonic()
        hover = client.result("textDocument/hover", Client.at(URI, 0, 6))
        latency = (time.monotonic() - started) * 1000
        report.check(f"hover answered in {latency:.0f}ms", latency < 2000,
                     f"{latency:.0f}ms")
        report.check("and reflects the newest text",
                     hover and "identity" in hover["contents"]["value"],
                     str(hover))

        report.section("redundant work")
        time.sleep(1.5)
        client.drain()
        client.notify("textDocument/didSave", {"textDocument": {"uri": URI}})
        time.sleep(1.0)
        report.check("an unchanged save re-runs nothing",
                     len(client.published(URI)) == 0,
                     f"got {len(client.published(URI))}")

        report.section("idle")
        client.drain()
        time.sleep(1.5)
        with client.lock:
            chatter = len(client.inbox)
        report.check("silent while idle", chatter == 0, f"{chatter} messages")

        report.section("a slow check does not stall the server")
        slow_uri = "file:///C:/work/slow.bz"
        client.drain()
        opened = time.monotonic()
        client.notify("textDocument/didOpen", {"textDocument": {
            "uri": slow_uri, "languageId": "benzene", "version": 1, "text": SLOW}})

        worst = 0.0
        for _ in range(5):
            started = time.monotonic()
            reply = client.request("textDocument/hover", Client.at(slow_uri, 0, 6))
            worst = max(worst, (time.monotonic() - started) * 1000)
            if reply is None:
                break
        report.check(f"requests stay quick throughout (worst {worst:.0f}ms)",
                     worst < 1500, f"{worst:.0f}ms")

        started = time.monotonic()
        client.notify("textDocument/didOpen", {"textDocument": {
            "uri": "file:///C:/work/other.bz", "languageId": "benzene",
            "version": 1, "text": "const x = 1\n"}})
        report.check("another file can still be opened",
                     (time.monotonic() - started) < 0.5)

        landed = client.await_diagnostics(slow_uri, timeout=120)
        report.check(f"the slow check lands ({time.monotonic() - opened:.1f}s)",
                     landed is not None, "never arrived")
        if landed:
            report.check("and reports on the right file",
                         landed["params"]["uri"] == slow_uri)

        hover = client.result("textDocument/hover", Client.at(slow_uri, 0, 6))
        report.check("afterwards hover uses the finished analysis",
                     hover and "fn_0" in hover["contents"]["value"], str(hover)[:120])

        report.check("exits cleanly", client.shutdown() == 0)
    finally:
        if client.process.poll() is None:
            client.process.kill()

    return report.done()


if __name__ == "__main__":
    raise SystemExit(1 if run() else 0)
