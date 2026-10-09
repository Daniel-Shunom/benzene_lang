-- Drives a real Neovim session against the Benzene plugin.
--
-- The Python suites talk to the server directly; this checks the half that
-- only exists inside the editor: filetype detection, the attach hooks, and
-- each feature as `vim.lsp` actually calls it.
--
--   nvim --headless --cmd "set rtp+=<repo>/lsp/editors/nvim" \
--        -c "edit <repo>/lsp/test/sample.bz" \
--        -c "luafile <repo>/lsp/test/nvim_check.lua"

local failures = 0

local function out(...)
  io.stdout:write(string.format(...) .. "\n")
end

local function check(label, ok, detail)
  out("    %s  %s%s", ok and "PASS" or "FAIL", label,
    (not ok and detail) and ("   " .. tostring(detail)) or "")
  if not ok then
    failures = failures + 1
  end
end

local function request(method, params)
  local replies = vim.lsp.buf_request_sync(0, method, params, 10000)
  for _, reply in pairs(replies or {}) do
    if reply.result ~= nil then
      return reply.result
    end
  end
end

local function document()
  return vim.lsp.util.make_text_document_params()
end

local function at(line, character)
  return { textDocument = document(), position = { line = line, character = character } }
end

local function whole_file()
  return {
    textDocument = document(),
    range = { start = { line = 0, character = 0 },
              ["end"] = { line = 200, character = 0 } },
  }
end

out("\nneovim")

vim.wait(30000, function()
  return #vim.lsp.get_clients({ bufnr = 0, name = "benzene" }) > 0
end, 100)

local client = vim.lsp.get_clients({ bufnr = 0, name = "benzene" })[1]
if not client then
  out("    FAIL  the server never attached")
  out("  see :LspLog, and check that lsp/build.cmd has been run")
  vim.cmd("cquit 1")
  return
end

out("  attach")
check("filetype is benzene", vim.bo.filetype == "benzene", vim.bo.filetype)
check("utf-8 position encoding", client.offset_encoding == "utf-8",
  client.offset_encoding)
check("inlay hints disabled by default", not vim.lsp.inlay_hint.is_enabled({ bufnr = 0 }))
check("foldexpr wired to the server",
  vim.wo.foldexpr:find("foldexpr") ~= nil, vim.wo.foldexpr)
check("commands registered",
  vim.fn.exists(":BenzeneLspStatus") == 2
  and vim.fn.exists(":BenzeneInlayHints") == 2
  and vim.fn.exists(":BenzeneCodeActions") == 2
  and vim.fn.exists(":BenzeneRestart") == 2)

out("  features through vim.lsp")
local hover = request("textDocument/hover", at(2, 6))
check("hover shows the signature",
  hover and hover.contents.value:find("identity%(x: Int%)") ~= nil,
  hover and hover.contents.value)

local references = request("textDocument/references",
  vim.tbl_extend("force", at(2, 6), { context = { includeDeclaration = true } }))
check("references finds both sites", references and #references == 2,
  references and #references)

local symbols = request("textDocument/documentSymbol", { textDocument = document() })
check("outline is hierarchical",
  symbols and #symbols == 4 and symbols[4].children and #symbols[4].children == 1,
  symbols and vim.inspect(vim.tbl_map(function(s) return s.name end, symbols)))

local completion = request("textDocument/completion", at(13, 2))
local items = completion and (completion.items or completion) or {}
local labels = {}
for _, item in ipairs(items) do
  labels[item.label] = true
end
check("completion offers in-scope names",
  labels["identity"] and labels["result"] and labels["func"],
  vim.inspect(vim.tbl_keys(labels)))

check("inlay hints returned",
  (request("textDocument/inlayHint", whole_file()) or {})[1] ~= nil)
check("folding ranges returned",
  #(request("textDocument/foldingRange", { textDocument = document() }) or {}) == 3)
check("document highlight returned",
  #(request("textDocument/documentHighlight", at(2, 6)) or {}) == 2)

out("  the annotate code action")
local actions = request("textDocument/codeAction", vim.tbl_extend("force",
  whole_file(), { context = { diagnostics = {} } }))
local annotate
local titles = {}
for _, action in ipairs(actions or {}) do
  titles[#titles + 1] = action.title
  if action.title:find("result") then
    annotate = action
  end
end
check("offered on an unannotated binding", annotate ~= nil, vim.inspect(titles))

-- `total` is inferred as a bare type variable, which has no spelling the
-- grammar accepts, so no edit may be offered for it.
check("not offered where the type cannot be written",
  not vim.tbl_contains(titles, "Annotate `total` as 't0"), vim.inspect(titles))

if annotate then
  vim.lsp.util.apply_workspace_edit(annotate.edit, client.offset_encoding)
  local line = vim.api.nvim_buf_get_lines(0, 12, 13, false)[1]
  check("applying it writes the type down",
    line and line:find("let result: Int") ~= nil, line)

  vim.wait(8000, function() return false end, 1500)
  check("and the result still checks clean", #vim.diagnostic.get(0) == 0,
    vim.inspect(vim.tbl_map(function(d) return d.message end,
      vim.diagnostic.get(0))))
end

out("  editing without saving")
vim.api.nvim_buf_set_lines(0, 0, -1, false, { "const a = 1", "const a = 2" })
vim.wait(10000, function() return #vim.diagnostic.get(0) > 0 end, 100)
local diagnostics = vim.diagnostic.get(0)
check("an unsaved edit produces diagnostics",
  #diagnostics == 1 and diagnostics[1].message:find("Duplicate") ~= nil,
  vim.inspect(vim.tbl_map(function(d) return d.message end, diagnostics)))
check("the buffer is still unsaved", vim.bo.modified)

vim.api.nvim_buf_set_lines(0, 0, -1, false, { "const a = 1", "const b = 2" })
vim.wait(10000, function() return #vim.diagnostic.get(0) == 0 end, 100)
check("fixing the error clears them", #vim.diagnostic.get(0) == 0)

out("  annotation quickfix")
vim.api.nvim_buf_set_lines(0, 0, -1, false, {
  "func use()", '  let b: Int = "text"', "  b", "end",
})
vim.wait(10000, function() return #vim.diagnostic.get(0) > 0 end, 100)
local fixes = request("textDocument/codeAction", {
  textDocument = document(),
  range = { start = { line = 1, character = 0 }, ["end"] = { line = 1, character = 30 } },
  context = { diagnostics = {} },
})
local fix
for _, action in ipairs(fixes or {}) do
  if action.kind == "quickfix" then fix = action end
end
check("incompatible annotations offer a quickfix", fix ~= nil, vim.inspect(fixes))
if fix then
  vim.lsp.util.apply_workspace_edit(fix.edit, client.offset_encoding)
  check("the quickfix removes the explicit annotation",
    vim.api.nvim_buf_get_lines(0, 1, 2, false)[1] == '  let b = "text"')
  vim.wait(10000, function() return #vim.diagnostic.get(0) == 0 end, 100)
  check("the quickfix clears the type error", #vim.diagnostic.get(0) == 0)
end

out("  commands")
vim.cmd("BenzeneInlayHints")
check("inlay hints toggle on", vim.lsp.inlay_hint.is_enabled({ bufnr = 0 }))
vim.cmd("BenzeneInlayHints")
check("and back off", not vim.lsp.inlay_hint.is_enabled({ bufnr = 0 }))

if failures == 0 then
  out("\nneovim suite passed")
  vim.cmd("qall!")
else
  out("\n" .. failures .. " failure(s) in the neovim suite")
  vim.cmd("cquit 1")
end
