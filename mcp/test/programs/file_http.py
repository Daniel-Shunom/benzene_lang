"""A syntax-free MCP request describing a hypothetical file/HTTP application.

Runtime operations are typed callback dependencies, not pretend native APIs.
"""


def node(kind, **fields):
    return {"kind": kind, **fields}


def named(name):
    return node("NamedType", name=name)


def applied(name, *args):
    return node("AppliedType", name=name, arguments=list(args))


def signature(params, returns):
    return node("FunctionType", parameters=params, returns=returns)


def ident(name):
    return node("Identifier", name=name)


def text(value):
    return node("Text", value=value)


def call(name, *args):
    return node("Call", identifier=name, arguments=list(args))


def param(name, kind):
    return {"param": name, "data_type": kind}


def function(name, params, returns, *body):
    return node("Function", identifier=name, param_list=params, return_type=returns, body=list(body))


def match(conditions, *branches):
    return node("CaseExpr", conditions=conditions,
                case_evals=[{"cases": patterns, "return_value": result} for patterns, result in branches])


def alias(name, target):
    return node("TypeDeclaration", identifier=name, alias_target=target)


def response(status, body):
    return call("Response", node("Integer", value=status), body)


def program():
    string, nil = named("String"), named("Nil")
    read_result, write_result = applied("Result", string, string), applied("Result", nil, string)
    wildcard = node("Wildcard")
    read_params = [param("readFile", named("Reader")), param("writeFile", named("Writer"))]
    return node("Module", data=[
        node("Comment", comment_type="SingleLine", data="Hypothetical I/O: a host supplies readFile, writeFile, and serve callbacks."),
        node("TypeDeclaration", identifier="Result", parameters=["a", "b"], members=[applied("Ok", named("a")), applied("Error", named("b"))]),
        node("TypeDeclaration", identifier="HttpRequest", members=[node("LabelledType", name="Request", fields=[
            {"name": "method", "data_type": string}, {"name": "path", "data_type": string}, {"name": "body", "data_type": string},
        ])]),
        node("TypeDeclaration", identifier="HttpResponse", members=[node("LabelledType", name="Response", fields=[
            {"name": "status", "data_type": named("Int")}, {"name": "body", "data_type": string},
        ])]),
        alias("Reader", signature([string], read_result)),
        alias("Writer", signature([string, string], write_result)),
        alias("HttpHandler", signature([named("HttpRequest")], named("HttpResponse"))),
        alias("Server", signature([named("Int"), named("HttpHandler")], write_result)),
        function("copyFile", read_params, write_result,
            match([call("readFile", text("input.txt"))],
                ([call("Ok", ident("contents"))], call("writeFile", text("output.txt"), ident("contents"))),
                ([call("Error", ident("reason"))], call("Error", ident("reason"))))),
        function("handleRequest", read_params + [param("request", named("HttpRequest"))], named("HttpResponse"),
            match([ident("request")],
                ([call("Request", ident("method"), ident("path"), ident("body"))],
                 match([ident("method"), ident("path")],
                    ([text("GET"), text("/")], match([call("readFile", text("output.txt"))],
                        ([call("Ok", ident("contents"))], response(200, ident("contents"))),
                        ([call("Error", ident("reason"))], response(500, ident("reason"))))),
                    ([text("POST"), text("/output")], match([call("writeFile", text("output.txt"), ident("body"))],
                        ([call("Ok", wildcard)], response(200, text("written"))),
                        ([call("Error", ident("reason"))], response(500, ident("reason"))))),
                    ([wildcard, wildcard], response(404, text("not found"))))))),
        function("app", read_params + [param("serve", named("Server"))], write_result,
            match([call("copyFile", ident("readFile"), ident("writeFile"))],
                ([call("Ok", wildcard)], call("serve", node("Integer", value=8080), node("Lambda",
                    param_list=[param("request", named("HttpRequest"))], return_type=named("HttpResponse"),
                    body=[call("handleRequest", ident("readFile"), ident("writeFile"), ident("request"))]))),
                ([call("Error", ident("reason"))], call("Error", ident("reason"))))),
    ])
