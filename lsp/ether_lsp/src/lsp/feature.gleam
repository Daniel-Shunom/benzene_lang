//// The language features, as pure functions over one analysis.
////
//// Nothing here does IO or holds state: each function takes the last scan of a
//// document (plus the buffer text where the cursor is ahead of the compiler)
//// and returns the JSON to reply with. That is what makes them testable
//// without a client, a compiler, or a running server.

import gleam/int
import gleam/json
import gleam/list
import gleam/option.{type Option, None, Some}
import gleam/result
import gleam/set
import gleam/string
import lsp/encode
import lsp/scan.{type Entry, type Scan, type Token}
import lsp/text

/// The index entry covering a 0-based LSP position.
pub fn entry_at(
  analysis: Scan,
  line: Int,
  character: Int,
) -> Result(Entry, Nil) {
  list.find(analysis.index, fn(entry) {
    scan.entry_covers(entry, line, character)
  })
}

/// Every occurrence of the same symbol. An entry the resolver could not bind
/// stands alone: two unknown names are not known to be the same name.
pub fn occurrences(analysis: Scan, entry: Entry) -> List(Entry) {
  case entry.def_line {
    0 -> [entry]
    _ -> list.filter(analysis.index, scan.same_symbol(entry, _))
  }
}

// --- hover ------------------------------------------------------------------

pub fn hover(analysis: Scan, line: Int, character: Int) -> json.Json {
  case entry_at(analysis, line, character) {
    Error(_) -> json.null()
    Ok(entry) ->
      json.object([
        #("contents", encode.markdown(encode.describe(entry))),
        #("range", encode.entry_span(entry)),
      ])
  }
}

// --- navigation -------------------------------------------------------------

pub fn definition(
  uri: String,
  analysis: Scan,
  line: Int,
  character: Int,
) -> json.Json {
  case entry_at(analysis, line, character) {
    Ok(entry) if entry.def_line != 0 ->
      json.object([
        #("uri", json.string(uri)),
        #("range", encode.definition_span(entry)),
      ])
    _ -> json.null()
  }
}

pub fn references(
  uri: String,
  analysis: Scan,
  line: Int,
  character: Int,
  include_declaration: Bool,
) -> json.Json {
  case entry_at(analysis, line, character) {
    Error(_) -> json.preprocessed_array([])
    Ok(entry) ->
      occurrences(analysis, entry)
      |> list.filter(fn(each) { include_declaration || !each.is_definition })
      |> list.map(fn(each) {
        json.object([
          #("uri", json.string(uri)),
          #("range", encode.entry_span(each)),
        ])
      })
      |> json.preprocessed_array
  }
}

pub fn document_highlight(
  analysis: Scan,
  line: Int,
  character: Int,
) -> json.Json {
  case entry_at(analysis, line, character) {
    Error(_) -> json.preprocessed_array([])
    Ok(entry) ->
      occurrences(analysis, entry)
      |> list.map(fn(each) {
        json.object([
          #("range", encode.entry_span(each)),
          // 3 = Write for the declaration, 2 = Read for every use.
          #(
            "kind",
            json.int(case each.is_definition {
              True -> 3
              False -> 2
            }),
          ),
        ])
      })
      |> json.preprocessed_array
  }
}

// --- outline ----------------------------------------------------------------

/// A nested outline. Bindings sit under the function that declares them,
/// which the compiler records as each entry's enclosing scope.
pub fn document_symbols(analysis: Scan) -> json.Json {
  let outline =
    list.filter(analysis.index, fn(entry) {
      entry.is_definition && encode.symbol_kind(entry.kind) != 0
    })

  json.preprocessed_array(symbols_under(outline, 0, 0, 0))
}

fn symbols_under(
  outline: List(Entry),
  scope_line: Int,
  scope_column: Int,
  depth: Int,
) -> List(json.Json) {
  // Depth is bounded defensively: the scope data comes from a separate process
  // and a cycle in it would otherwise loop forever inside the editor's
  // request.
  case depth > 32 {
    True -> []
    False ->
      outline
      |> list.filter(fn(entry) {
        entry.scope_line == scope_line && entry.scope_column == scope_column
      })
      |> list.map(fn(entry) {
        json.object([
          #("name", json.string(entry.name)),
          #(
            "detail",
            json.string(case entry.detail {
              "" -> entry.inferred
              detail -> detail
            }),
          ),
          #("kind", json.int(encode.symbol_kind(entry.kind))),
          #("range", encode.entry_span(entry)),
          #("selectionRange", encode.entry_span(entry)),
          #(
            "children",
            json.preprocessed_array(symbols_under(
              outline,
              entry.line,
              entry.column,
              depth + 1,
            )),
          ),
        ])
      })
  }
}

// --- completion -------------------------------------------------------------

/// Everything in scope, plus the keywords.
///
/// After a `:` or `:>` only types can follow, so that is all this offers there.
/// Otherwise module-level names are always visible, and locals are narrowed to
/// the function the cursor is in.
pub fn completion(
  analysis: Scan,
  source: String,
  line: Int,
  character: Int,
) -> json.Json {
  let items = case text.in_type_position(source, line, character) {
    True -> type_completions(analysis)
    False -> value_completions(analysis, line)
  }

  json.object([
    #("isIncomplete", json.bool(False)),
    #("items", json.preprocessed_array(items)),
  ])
}

/// A proposed completion, before encoding. The label is kept apart so
/// duplicates can be dropped by name rather than by rendered shape.
type Candidate {
  Candidate(label: String, kind: String, detail: String, rank: String)
}

fn type_completions(analysis: Scan) -> List(json.Json) {
  let declared =
    analysis.index
    |> list.filter(fn(entry) { entry.is_definition && entry.kind == "Type" })
    |> list.map(fn(entry) {
      Candidate(entry.name, entry.kind, entry.inferred, "0")
    })

  let builtin =
    text.builtin_types
    |> list.map(fn(name) { Candidate(name, "Type", "built-in type", "1") })

  encode_candidates(dedupe(list.append(declared, builtin)))
}

fn value_completions(analysis: Scan, line: Int) -> List(json.Json) {
  let scope = enclosing_scope(analysis, line)

  let visible =
    analysis.index
    |> list.filter(fn(entry) { entry.is_definition })
    |> list.filter(fn(entry) { in_scope(entry, scope) })
    |> list.map(fn(entry) {
      let detail = case entry.detail {
        "" -> entry.inferred
        detail -> detail
      }
      // Locals sort above module-level names: the nearer the declaration, the
      // more likely it is what is being typed.
      let rank = case entry.scope_line {
        0 -> "1"
        _ -> "0"
      }
      Candidate(entry.name, entry.kind, detail, rank)
    })

  let words =
    text.keywords
    |> list.map(fn(word) { Candidate(word, "Keyword", "keyword", "2") })

  let types =
    text.builtin_types
    |> list.map(fn(name) { Candidate(name, "Type", "built-in type", "3") })

  encode_candidates(dedupe(list.flatten([visible, words, types])))
}

/// Module-level declarations are visible everywhere; locals only inside the
/// function that declares them.
fn in_scope(entry: Entry, scope: #(Int, Int)) -> Bool {
  entry.scope_line == 0 || #(entry.scope_line, entry.scope_column) == scope
}

/// Which function the cursor is inside, as that function's name token.
///
/// Derived from the last indexed entry at or above the cursor: if that entry is
/// a function declaration the cursor is in its body, otherwise the cursor
/// shares that entry's scope. It is an approximation -- the index records
/// points, not body extents -- but it is right wherever anything is declared.
fn enclosing_scope(analysis: Scan, line: Int) -> #(Int, Int) {
  analysis.index
  |> list.filter(fn(entry) { entry.line <= line + 1 })
  |> list.last
  |> result.map(fn(entry) {
    case entry.kind == "Function" && entry.is_definition {
      True -> #(entry.line, entry.column)
      False -> #(entry.scope_line, entry.scope_column)
    }
  })
  |> result.unwrap(#(0, 0))
}

fn encode_candidates(candidates: List(Candidate)) -> List(json.Json) {
  list.map(candidates, fn(candidate) {
    json.object([
      #("label", json.string(candidate.label)),
      #("kind", json.int(encode.completion_kind(candidate.kind))),
      #("detail", json.string(candidate.detail)),
      #("sortText", json.string(candidate.rank <> candidate.label)),
    ])
  })
}

/// Keeps the first candidate for each label.
///
/// Ordering puts declarations before keywords, so a name the user defined wins
/// over a reserved word -- and `Nil`, which is both a keyword and a built-in
/// type, is offered once rather than twice.
fn dedupe(candidates: List(Candidate)) -> List(Candidate) {
  let #(kept, _) =
    list.fold(candidates, #([], set.new()), fn(state, candidate) {
      let #(kept, seen) = state
      case set.contains(seen, candidate.label) {
        True -> state
        False -> #([candidate, ..kept], set.insert(seen, candidate.label))
      }
    })
  list.reverse(kept)
}

// --- inlay hints ------------------------------------------------------------

/// Where an inferred type would be written, and what it would say.
///
/// Shared by inlay hints and the "annotate" code action: a hint shows the text
/// at that spot, the action inserts it there. Having one source for both is
/// what guarantees that accepting the action produces the text the hint
/// promised.
type Annotation {
  Annotation(
    line: Int,
    character: Int,
    /// Exactly the characters to insert, punctuation included.
    text: String,
    /// The type on its own, for prose like a code-action title.
    type_name: String,
  )
}

/// `None` when the entry is not a kind that takes an annotation, or when the
/// position cannot be worked out from the tokens.
fn annotation(analysis: Scan, entry: Entry) -> Result(Annotation, Nil) {
  case entry.kind {
    "Binding" | "FuncParam" ->
      Ok(Annotation(
        entry.line - 1,
        entry.column - 1 + entry.length,
        ": " <> entry.inferred,
        entry.inferred,
      ))

    // A return type belongs after the parameter list, not after the name, so
    // this needs the closing paren the lexer saw.
    "Function" ->
      case closing_paren(analysis, entry) {
        Ok(token) ->
          Ok(Annotation(
            token.line - 1,
            token.column - 1 + token.length,
            " :> " <> return_of(entry.inferred),
            return_of(entry.inferred),
          ))
        Error(_) -> Error(Nil)
      }

    _ -> Error(Nil)
  }
}

/// The declarations in `[from_line, to_line]` that the user left unannotated.
fn unannotated(analysis: Scan, from_line: Int, to_line: Int) -> List(Entry) {
  list.filter(analysis.index, fn(entry) {
    entry.is_definition
    && !entry.annotated
    && entry.inferred != ""
    && entry.line - 1 >= from_line
    && entry.line - 1 <= to_line
  })
}

/// Shows what was inferred where the user did not write it: on `let` bindings,
/// on unannotated parameters, and on a function's return type.
pub fn inlay_hints(analysis: Scan, from_line: Int, to_line: Int) -> json.Json {
  unannotated(analysis, from_line, to_line)
  |> list.filter_map(fn(entry) {
    use found <- result.map(annotation(analysis, entry))
    json.object([
      #("position", encode.position(found.line, found.character)),
      #("label", json.string(found.text)),
      // 1 = Type. Parameter-name hints would need call sites, which this index
      // does not record.
      #("kind", json.int(1)),
      #("paddingLeft", json.bool(False)),
      #("paddingRight", json.bool(False)),
    ])
  })
  |> json.preprocessed_array
}

/// Offers to write down the type the checker worked out.
///
/// This is the one refactor the compiler can supply on its own: it already
/// knows the type, and it already knows the user did not write it.
pub fn code_actions(
  uri: String,
  analysis: Scan,
  from_line: Int,
  to_line: Int,
) -> json.Json {
  unannotated(analysis, from_line, to_line)
  |> list.filter_map(fn(entry) {
    use found <- result.map(annotation(analysis, entry))

    // A zero-width range is an insertion at that point.
    let at = encode.position(found.line, found.character)
    let edit =
      json.object([
        #("range", json.object([#("start", at), #("end", at)])),
        #("newText", json.string(found.text)),
      ])

    json.object([
      #(
        "title",
        json.string("Annotate `" <> entry.name <> "` as " <> found.type_name),
      ),
      #("kind", json.string("refactor.rewrite")),
      #(
        "edit",
        json.object([
          #("changes", json.object([#(uri, json.preprocessed_array([edit]))])),
        ]),
      ),
    ])
  })
  |> json.preprocessed_array
}

fn closing_paren(analysis: Scan, entry: Entry) -> Result(Token, Nil) {
  analysis.tokens
  |> list.drop_while(fn(token) {
    token.line < entry.line
    || { token.line == entry.line && token.column <= entry.column }
  })
  |> list.find(fn(token) { token.kind == "RParen" })
}

/// `Fn(Int) :> Int` -> `Int`. Splitting on the last arrow is what makes a
/// higher-order parameter like `Fn(Fn(Int) :> Int) :> Bool` come out right.
fn return_of(rendered: String) -> String {
  case list.last(string.split(rendered, " :> ")) {
    Ok(tail) -> tail
    Error(_) -> rendered
  }
}

// --- signature help ---------------------------------------------------------

/// The signature of the call the cursor is inside, with the argument being
/// typed marked active.
pub fn signature_help(analysis: Scan, line: Int, character: Int) -> json.Json {
  case open_call(analysis, line, character) {
    Error(_) -> json.null()
    Ok(#(callee, argument)) ->
      case find_signature(analysis, callee) {
        Error(_) -> json.null()
        Ok(detail) ->
          json.object([
            #(
              "signatures",
              json.preprocessed_array([
                json.object([
                  #("label", json.string(detail)),
                  #(
                    "parameters",
                    json.preprocessed_array(
                      list.map(parameters_of(detail), fn(parameter) {
                        json.object([#("label", json.string(parameter))])
                      }),
                    ),
                  ),
                ]),
              ]),
            ),
            #("activeSignature", json.int(0)),
            #("activeParameter", json.int(argument)),
          ])
      }
  }
}

/// Walks the tokens up to the cursor keeping a stack of open calls, and
/// returns the innermost one still open: the name called, and how many commas
/// have been typed inside it.
fn open_call(
  analysis: Scan,
  line: Int,
  character: Int,
) -> Result(#(Token, Int), Nil) {
  let before =
    list.take_while(analysis.tokens, fn(token) {
      token.line - 1 < line
      || { token.line - 1 == line && token.column - 1 < character }
    })

  let #(stack, _) =
    list.fold(before, #([], None), fn(state, token) {
      let #(stack, previous) = state
      let next = case token.kind {
        "LParen" ->
          case previous {
            Some(name) -> [#(name, 0), ..stack]
            // A parenthesis with no name in front of it is a grouping, not a
            // call; it still has to be tracked so its `)` pops the right frame.
            None -> [#(token, -1), ..stack]
          }

        "RParen" ->
          case stack {
            [_, ..rest] -> rest
            [] -> stack
          }

        "Delimitter" ->
          case stack {
            [#(name, count), ..rest] -> [#(name, count + 1), ..rest]
            [] -> stack
          }

        _ -> stack
      }

      let carried = case token.kind {
        "Identifier" -> Some(token)
        _ -> None
      }
      #(next, carried)
    })

  case stack {
    [#(_, -1), ..] -> Error(Nil)
    [frame, ..] -> Ok(frame)
    [] -> Error(Nil)
  }
}

fn find_signature(analysis: Scan, callee: Token) -> Result(String, Nil) {
  use entry <- result.try(
    list.find(analysis.index, fn(entry) {
      entry.line == callee.line && entry.column == callee.column
    }),
  )

  // The occurrence at a call site carries no signature; the declaration does.
  occurrences(analysis, entry)
  |> list.find(fn(each) { each.detail != "" })
  |> result.map(fn(each) { each.detail })
}

/// `identity(x: Int) :> Int` -> `["x: Int"]`.
fn parameters_of(detail: String) -> List(String) {
  case string.split_once(detail, "(") {
    Error(_) -> []
    Ok(#(_, rest)) ->
      case string.split_once(rest, ")") {
        Error(_) -> []
        Ok(#(inside, _)) ->
          case string.trim(inside) {
            "" -> []
            text -> list.map(string.split(text, ", "), string.trim)
          }
      }
  }
}

// --- folding ----------------------------------------------------------------

/// Folds `func`/`case` bodies and brace-delimited scopes, paired from the
/// token stream rather than from indentation.
pub fn folding_ranges(analysis: Scan) -> json.Json {
  let #(ranges, _) =
    list.fold(analysis.tokens, #([], []), fn(state, token) {
      let #(ranges, stack) = state
      case token.kind {
        "FuncStart" | "Case" | "LBrace" -> #(ranges, [token, ..stack])

        "EndStmt" | "RBrace" ->
          case stack {
            [open, ..rest] ->
              case token.line > open.line {
                True -> #([#(open.line, token.line), ..ranges], rest)
                // A one-line block has nothing to fold.
                False -> #(ranges, rest)
              }
            [] -> state
          }

        _ -> state
      }
    })

  ranges
  |> list.map(fn(range) {
    let #(start, end) = range
    json.object([
      #("startLine", json.int(start - 1)),
      // The closing line stays visible when folded, which is what makes a
      // folded `func` still show its `end`.
      #("endLine", json.int(end - 2)),
      #("kind", json.string("region")),
    ])
  })
  |> json.preprocessed_array
}

// --- rename -----------------------------------------------------------------

pub fn prepare_rename(analysis: Scan, line: Int, character: Int) -> json.Json {
  case entry_at(analysis, line, character) {
    // Renaming something the resolver could not bind would silently miss its
    // other occurrences, so it is refused rather than done badly.
    Ok(entry) if entry.def_line != 0 ->
      json.object([
        #("range", encode.entry_span(entry)),
        #("placeholder", json.string(entry.name)),
      ])
    _ -> json.null()
  }
}

/// All the edits for a rename, or a message explaining why there are none.
pub fn rename(
  uri: String,
  analysis: Scan,
  line: Int,
  character: Int,
  new_name: String,
) -> Result(json.Json, String) {
  use <- guard_name(new_name)

  case entry_at(analysis, line, character) {
    Error(_) -> Error("there is nothing to rename here")
    Ok(entry) if entry.def_line == 0 ->
      Error(
        "`"
        <> entry.name
        <> "` is not resolved, so its other uses cannot be found",
      )
    Ok(entry) -> {
      let edits =
        occurrences(analysis, entry)
        |> list.map(fn(each) {
          json.object([
            #("range", encode.entry_span(each)),
            #("newText", json.string(new_name)),
          ])
        })

      Ok(
        json.object([
          #("changes", json.object([#(uri, json.preprocessed_array(edits))])),
        ]),
      )
    }
  }
}

fn guard_name(
  new_name: String,
  continue: fn() -> Result(json.Json, String),
) -> Result(json.Json, String) {
  case text.is_identifier(new_name) {
    True -> continue()
    False -> Error("`" <> new_name <> "` is not a valid Benzene identifier")
  }
}

// --- diagnostics ------------------------------------------------------------

pub fn diagnostics(analysis: Scan) -> List(json.Json) {
  list.map(analysis.diagnostics, encode_diagnostic)
}

fn encode_diagnostic(diagnostic: scan.Diagnostic) -> json.Json {
  let related =
    list.map(diagnostic.related, fn(each) {
      json.object([
        #(
          "location",
          json.object([
            #("range", encode.span(each.line, each.column, each.length)),
          ]),
        ),
        #("message", json.string(each.message)),
      ])
    })

  json.object([
    #(
      "range",
      encode.span(diagnostic.line, diagnostic.column, diagnostic.length),
    ),
    #("severity", json.int(severity(diagnostic.severity))),
    #("source", json.string("ether/" <> diagnostic.phase)),
    #("message", json.string(diagnostic.message)),
    #("relatedInformation", json.preprocessed_array(related)),
  ])
}

fn severity(level: String) -> Int {
  case level {
    "error" -> 1
    "warning" -> 2
    _ -> 3
  }
}

/// Shown when the compiler itself could not be run, so the problem is visible
/// in the editor instead of only in the log.
pub fn tooling_diagnostic(reason: String) -> json.Json {
  json.object([
    #("range", encode.span(1, 1, 1)),
    #("severity", json.int(1)),
    #("source", json.string("ether-lsp")),
    #("message", json.string("could not run the ether compiler: " <> reason)),
  ])
}

/// Formats a count for the log line written after each analysis.
pub fn summary(analysis: Scan, elapsed_ms: Int) -> String {
  int.to_string(list.length(analysis.diagnostics))
  <> " diagnostic(s), "
  <> int.to_string(list.length(analysis.index))
  <> " symbol(s) in "
  <> int.to_string(elapsed_ms)
  <> "ms"
}

/// Exposed for the server's unresolved-name check in `prepare_rename`.
pub fn is_resolved(entry: Entry) -> Bool {
  entry.def_line != 0
}

/// Exposed so the server can report what it knows without reaching inside.
pub fn option_to_result(value: Option(a)) -> Result(a, Nil) {
  option.to_result(value, Nil)
}
