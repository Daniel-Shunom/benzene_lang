"""Every language feature, driven over the wire."""

from client import Client, Report

URI = "file:///C:/work/features.bz"

SOURCE = """const greeting: String = "hello"

func identity(x: Int) :> Int
  x
end

func pair(a, b)
  let total = a
  total
end

func main()
  let result = identity(42)
  result
end
"""

CAPABILITIES = [
    "hoverProvider",
    "definitionProvider",
    "referencesProvider",
    "documentHighlightProvider",
    "documentSymbolProvider",
    "foldingRangeProvider",
    "inlayHintProvider",
    "renameProvider",
    "completionProvider",
    "signatureHelpProvider",
    "semanticTokensProvider",
    "codeActionProvider",
]


def run():
    report = Report("features")
    client = Client()
    try:
        init = client.initialize()
        caps = init["result"]["capabilities"]

        report.section("capabilities")
        for name in CAPABILITIES:
            report.check(name, name in caps)
        report.check("utf-8 encoding negotiated",
                     caps.get("positionEncoding") == "utf-8",
                     caps.get("positionEncoding"))

        published = client.open(URI, SOURCE)

        report.section("diagnostics")
        report.check("a clean file reports none",
                     published["params"]["diagnostics"] == [],
                     str(published["params"]["diagnostics"]))

        report.section("hover")
        hover = client.result("textDocument/hover", Client.at(URI, 2, 6))
        report.check("shows the full signature",
                     "identity(x: Int) :> Int" in hover["contents"]["value"],
                     hover["contents"]["value"])

        report.section("navigation")
        definition = client.result("textDocument/definition", Client.at(URI, 12, 17))
        report.check("a call resolves to its declaration",
                     definition and definition["range"]["start"]["line"] == 2,
                     str(definition))

        refs = client.result("textDocument/references", dict(
            Client.at(URI, 2, 6), context={"includeDeclaration": True}))
        lines = sorted(r["range"]["start"]["line"] for r in refs)
        report.check("references finds declaration and call", lines == [2, 12], str(lines))

        refs = client.result("textDocument/references", dict(
            Client.at(URI, 2, 6), context={"includeDeclaration": False}))
        lines = sorted(r["range"]["start"]["line"] for r in refs)
        report.check("the declaration can be excluded", lines == [12], str(lines))

        highlights = client.result("textDocument/documentHighlight", Client.at(URI, 2, 6))
        kinds = sorted(h["kind"] for h in highlights)
        report.check("highlight marks one write and one read", kinds == [2, 3], str(kinds))

        report.section("outline")
        symbols = client.result("textDocument/documentSymbol",
                                {"textDocument": {"uri": URI}})
        names = [s["name"] for s in symbols]
        report.check("top level is module scope",
                     names == ["greeting", "identity", "pair", "main"], str(names))
        main = [s for s in symbols if s["name"] == "main"][0]
        report.check("locals nest under their function",
                     [c["name"] for c in main["children"]] == ["result"],
                     str(main["children"]))

        report.section("completion")
        items = client.result("textDocument/completion", Client.at(URI, 13, 2))["items"]
        labels = [i["label"] for i in items]
        report.check("offers functions", "identity" in labels)
        report.check("offers keywords", "func" in labels)
        report.check("offers the local binding", "result" in labels)
        report.check("hides another function's local", "total" not in labels, str(labels))
        report.check("offers each label once",
                     len(labels) == len(set(labels)),
                     str([l for l in labels if labels.count(l) > 1]))

        types = client.result("textDocument/completion", Client.at(URI, 0, 15))["items"]
        type_labels = [i["label"] for i in types]
        report.check("after a colon, only types",
                     "Int" in type_labels and "identity" not in type_labels,
                     str(type_labels))

        report.section("inlay hints")
        hints = client.result("textDocument/inlayHint", {
            "textDocument": {"uri": URI},
            "range": {"start": {"line": 0, "character": 0},
                      "end": {"line": 40, "character": 0}}})
        labelled = [(h["position"]["line"], h["label"]) for h in hints]
        report.check("types an unannotated binding",
                     (12, ": Int") in labelled, str(labelled))
        report.check("types an unannotated return",
                     any(" :> " in label for _, label in labelled), str(labelled))
        report.check("leaves annotated code alone",
                     not any(line == 0 for line, _ in labelled), str(labelled))

        report.section("code actions")
        actions = client.result("textDocument/codeAction", {
            "textDocument": {"uri": URI},
            "range": {"start": {"line": 12, "character": 6},
                      "end": {"line": 12, "character": 6}},
            "context": {"diagnostics": []}})
        report.check("offers to write the inferred type down",
                     actions and any("as Int" in a["title"] for a in actions),
                     str(actions))

        report.section("signature help")
        signature = client.result("textDocument/signatureHelp", Client.at(URI, 12, 25))
        report.check("reports the callee",
                     signature and "identity" in signature["signatures"][0]["label"],
                     str(signature))

        report.section("folding")
        folds = client.result("textDocument/foldingRange", {"textDocument": {"uri": URI}})
        starts = sorted(f["startLine"] for f in folds)
        report.check("folds each function", starts == [2, 6, 11], str(starts))

        report.section("rename")
        prepared = client.result("textDocument/prepareRename", Client.at(URI, 2, 6))
        report.check("prepare offers the placeholder",
                     prepared and prepared["placeholder"] == "identity", str(prepared))

        renamed = client.request("textDocument/rename",
                                 dict(Client.at(URI, 2, 6), newName="apply"))
        edits = renamed["result"]["changes"][URI]
        report.check("renames every occurrence", len(edits) == 2, str(edits))

        for bad in ("end", "has space", "2start", ""):
            reply = client.request("textDocument/rename",
                                   dict(Client.at(URI, 2, 6), newName=bad))
            report.check(f"refuses {bad!r}", "error" in reply, str(reply))

        report.section("semantic tokens")
        tokens = client.result("textDocument/semanticTokens/full",
                               {"textDocument": {"uri": URI}})
        report.check("returns a well-formed token array",
                     tokens and len(tokens["data"]) % 5 == 0 and tokens["data"],
                     str(len(tokens["data"]) if tokens else None))

        report.section("shutdown")
        report.check("exits cleanly", client.shutdown() == 0)
    finally:
        if client.process.poll() is None:
            client.process.kill()

    return report.done()


if __name__ == "__main__":
    raise SystemExit(1 if run() else 0)
