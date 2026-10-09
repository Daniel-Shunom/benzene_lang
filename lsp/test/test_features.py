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


def apply_action(source, action, uri):
    """Apply the server's edits exactly as an editor would (ASCII fixtures)."""
    lines = source.splitlines(keepends=True)
    def offset(position):
        return sum(len(line) for line in lines[:position["line"]]) + position["character"]
    edits = action["edit"]["changes"][uri]
    for edit in sorted(edits, key=lambda e: offset(e["range"]["start"]), reverse=True):
        start = offset(edit["range"]["start"])
        end = offset(edit["range"]["end"])
        source = source[:start] + edit["newText"] + source[end:]
    return source


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

        report.section("generic type parameters")
        generic_uri = "file:///C:/work/generic-types.bz"
        generic_source = (
            "type Result(a, b) {\n  Ok(a)\n  Error(b)\n}\n"
            "func get(r: Result(Int, String)) :> Int\n"
            "  case r:\n    Ok(x) :> x\n  end\nend\n"
        )
        published = client.open(generic_uri, generic_source)
        report.check("generic constructor fields type check",
                     published["params"]["diagnostics"] == [], str(published))
        hover = client.result("textDocument/hover", Client.at(generic_uri, 1, 5))
        report.check("hover identifies a generic parameter",
                     hover and "type parameter" in hover["contents"]["value"], str(hover))
        definition = client.result("textDocument/definition", Client.at(generic_uri, 1, 5))
        report.check("field parameter navigates to the parent parameter",
                     definition and definition["range"]["start"] == {"line": 0, "character": 12},
                     str(definition))
        renamed = client.result("textDocument/rename",
                                dict(Client.at(generic_uri, 1, 5), newName="value"))
        report.check("rename updates the declaration and its field reference",
                     renamed and len(renamed["changes"][generic_uri]) == 2, str(renamed))
        hover = client.result("textDocument/hover", Client.at(generic_uri, 6, 13))
        report.check("pattern-bound values show the instantiated field type",
                     hover and "Int" in hover["contents"]["value"], str(hover))
        tokens = client.result("textDocument/semanticTokens/full",
                               {"textDocument": {"uri": generic_uri}})
        parameter_kind = caps["semanticTokensProvider"]["legend"]["tokenTypes"].index("typeParameter")
        report.check("generic parameters receive typeParameter highlighting",
                     tokens and tokens["data"][3::5].count(parameter_kind) == 4, str(tokens))
        invalid_uri = "file:///C:/work/invalid-generic.bz"
        published = client.open(invalid_uri, "type Box(a) {\n  Wrap(c)\n}\n")
        report.check("undeclared parameters produce editor diagnostics",
                     any("Type `c` is not defined" in d["message"]
                         for d in published["params"]["diagnostics"]), str(published))

        report.section("generic function aliases")
        alias_uri = "file:///C:/work/generic-alias.bz"
        published = client.open(alias_uri,
            "type Handler(data) = Fn(data) :> String\n"
            "func foo(x: Int) :> String\n  \"ok\"\nend\n"
            "func use() :> String\n  let b: Handler(Int) = foo\n  b(1)\nend\n")
        report.check("function signature aliases accept matching functions",
                     published["params"]["diagnostics"] == [], str(published))
        hover = client.result("textDocument/hover", Client.at(alias_uri, 5, 6))
        report.check("alias bindings show the substituted function signature",
                     hover and "Int" in hover["contents"]["value"]
                     and "String" in hover["contents"]["value"], str(hover))
        hover = client.result("textDocument/hover", Client.at(alias_uri, 5, 10))
        report.check("alias application hover shows its substituted signature",
                     hover and "Int" in hover["contents"]["value"]
                     and "String" in hover["contents"]["value"], str(hover))

        report.section("case pattern validation")
        case_uri = "file:///C:/work/case-patterns.bz"
        published = client.open(case_uri,
            "type Result(a, b) { Ok(a) Error(b) }\n"
            "func copy(r: Result(Int, String)) :> Result(Int, String)\n"
            "  case r:\n    Ok(x) :> Ok(x)\n    Error(x) :> Error(x)\n  end\nend\n")
        report.check("branch-local names and constructor results type check",
                     published["params"]["diagnostics"] == [], str(published))
        case_error_uri = "file:///C:/work/invalid-case-patterns.bz"
        published = client.open(case_error_uri,
            "func wrong(x: Int)\n  case x:\n    \"text\" :> 1\n  end\nend\n")
        report.check("incompatible case patterns produce diagnostics",
                     bool(published["params"]["diagnostics"]), str(published))

        report.section("annotation code actions")
        mismatch_uri = "file:///C:/work/mismatched-annotation.bz"
        mismatch_source = "func use()\n  let b: Int = \"text\"\n  b\nend\n"
        published = client.open(mismatch_uri, mismatch_source)
        actions = client.result("textDocument/codeAction", {
            "textDocument": {"uri": mismatch_uri},
            "range": {"start": {"line": 1, "character": 0}, "end": {"line": 1, "character": 30}},
            "context": {"diagnostics": published["params"]["diagnostics"]}})
        remove = next((a for a in actions if a["kind"] == "quickfix"), None)
        report.check("an incompatible annotation offers inference as a quickfix", remove is not None, str(actions))
        if remove:
            client.drain()
            client.change(mismatch_uri, apply_action(mismatch_source, remove, mismatch_uri))
            published = client.await_diagnostics(mismatch_uri)
            report.check("the annotation quickfix clears the mismatch",
                         published["params"]["diagnostics"] == [], str(published))

        alias_source = (
            "type Handler(data) = Fn(data) :> String\n"
            "func foo(x: Int) :> String\n  \"ok\"\nend\n"
            "func use()\n  let b: Handler(Int) = foo\n  b(1)\nend\n")
        expand_uri = "file:///C:/work/expand-alias.bz"
        client.open(expand_uri, alias_source)
        actions = client.result("textDocument/codeAction", {
            "textDocument": {"uri": expand_uri},
            "range": {"start": {"line": 5, "character": 0}, "end": {"line": 5, "character": 40}},
            "context": {"diagnostics": []}})
        expand = next((a for a in actions if a["title"].startswith("Expand type alias")), None)
        report.check("a generic alias annotation offers expansion", expand is not None, str(actions))
        if expand:
            client.drain()
            expanded = apply_action(alias_source, expand, expand_uri)
            client.change(expand_uri, expanded)
            published = client.await_diagnostics(expand_uri)
            report.check("expanded aliases produce valid matching function annotations",
                         "let b: Fn(Int) :> String = foo" in expanded
                         and published["params"]["diagnostics"] == [], str(published))

        inferred_uri = "file:///C:/work/annotate-function.bz"
        inferred_source = "func foo(x: Int) :> String\n  \"ok\"\nend\nfunc use()\n  let b = foo\n  b(1)\nend\n"
        client.open(inferred_uri, inferred_source)
        actions = client.result("textDocument/codeAction", {
            "textDocument": {"uri": inferred_uri},
            "range": {"start": {"line": 4, "character": 0}, "end": {"line": 4, "character": 20}},
            "context": {"diagnostics": []}})
        annotate_fn = next((a for a in actions if "Fn(Int) :> String" in a["title"]), None)
        report.check("function-valued bindings offer concrete signature annotations", annotate_fn is not None, str(actions))
        if annotate_fn:
            client.drain()
            client.change(inferred_uri, apply_action(inferred_source, annotate_fn, inferred_uri))
            published = client.await_diagnostics(inferred_uri)
            report.check("function annotation edits re-check cleanly",
                         published["params"]["diagnostics"] == [], str(published))

        report.section("diagnostic wording")
        undefined_uri = "file:///C:/work/undefined-names.bz"
        published = client.open(undefined_uri,
            "func use()\n  missing()\n  let b = absent\nend\n")
        messages = [d["message"] for d in published["params"]["diagnostics"]]
        report.check("undefined functions and identifiers are named clearly",
                     "Function `missing` is not defined" in messages
                     and "Identifier `absent` is not defined" in messages, str(messages))
        generic_error_uri = "file:///C:/work/alias-mismatch.bz"
        published = client.open(generic_error_uri,
            "type Handler(data) = Fn(data) :> String\n"
            "func foo(x: String) :> String\n  x\nend\n"
            "func use()\n  let b: Handler(Int) = foo\nend\n")
        report.check("alias mismatches identify the declared and expanded types",
                     any("Handler(Int)" in d["message"] and "expands to Fn(Int) :> String" in d["message"]
                         for d in published["params"]["diagnostics"]), str(published))

        report.section("shutdown")
        report.check("exits cleanly", client.shutdown() == 0)
    finally:
        if client.process.poll() is None:
            client.process.kill()

    return report.done()


if __name__ == "__main__":
    raise SystemExit(1 if run() else 0)
