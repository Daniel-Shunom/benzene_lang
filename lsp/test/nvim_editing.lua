-- Checks the editing behaviour the plugin adds: closing blocks as you type,
-- and the comment highlighting that the language server does not supply.
--
--   nvim --headless --clean --cmd "set rtp+=<repo>/lsp/editors/nvim" \
--        -c "luafile <repo>/lsp/test/nvim_editing.lua"

local blocks = require("benzene.blocks")

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

--- Lets the scheduled insertion land.
---
--- The watcher cannot edit the buffer from inside the change callback, so the
--- work is scheduled and the event loop has to turn before it shows.
local function settle()
  vim.wait(300, function() return false end, 50)
end

local function fresh()
  vim.cmd("enew!")
  vim.bo.filetype = "benzene"
end

local function contents()
  return vim.api.nvim_buf_get_lines(0, 0, -1, false)
end

local function joined(lines)
  return table.concat(lines, " / ")
end

--- Types `text` then Enter, as real keystrokes, into an empty buffer.
---
--- One `nvim_feedkeys` call for the whole thing: mode "x" ends insert mode at
--- the end of each call the way `:normal!` does, so a second call would be read
--- as normal-mode commands.
local function typed(text)
  fresh()
  vim.api.nvim_feedkeys(
    vim.api.nvim_replace_termcodes("i" .. text .. "<CR><Esc>", true, false, true),
    "x", false)
  settle()
  return contents()
end

--- Does to the buffer exactly what pressing Enter at the end of `row` does.
---
--- Used where a test needs more than one step. Keystrokes cannot give that:
--- mode "x" runs the whole sequence before the event loop turns, so a scheduled
--- insertion would only ever see the final state.
local function split_after(row)
  vim.api.nvim_buf_set_lines(0, row, row, false, { "" })
  vim.api.nvim_win_set_cursor(0, { row + 1, 0 })
  settle()
end

local function start_with(lines, row)
  fresh()
  vim.api.nvim_buf_set_lines(0, 0, -1, false, lines)
  vim.api.nvim_win_set_cursor(0, { row, #lines[row] })
end

--- True when nothing was appended: the opener, and at most a blank line.
local function nothing_added(result)
  return #result <= 2 and (result[2] == nil or result[2]:match("^%s*$") ~= nil)
end

out("\nediting")

out("  recognising openers")
check("func", blocks.opens_block("func greet()"))
check("case", blocks.opens_block("case value:"))
check("a lambda binding", blocks.opens_block("let f = Fn(x) :> Int"))
check("an indented opener", blocks.opens_block("    func inner()"))
check("not a comment", not blocks.opens_block("Cmt func looks like one"))
check("not a type expression",
  not blocks.opens_block("type FooBar = Fn(List(Int)) :> String"))
check("not an ordinary statement", not blocks.opens_block("const x = 1"))
check("not a line that already closed", not blocks.opens_block("func f() 1 end"))
check("not a name merely containing func",
  not blocks.opens_block("let refunction = 1"))

out("  typing a block")

local lines = typed("func greet()")
check("func gets its end", lines[3] == "end", joined(lines))
check("and the cursor line is indented", lines[2] == "  ", joined(lines))

lines = typed("case value:")
check("case gets its end", lines[3] == "end", joined(lines))

lines = typed("let f = Fn(x) :> Int")
check("a lambda gets its end", lines[3] == "end", joined(lines))

out("  what must not get an end")
check("a comment does not", nothing_added(typed("Cmt func looks like one")))
check("a type expression using Fn does not",
  nothing_added(typed("type FooBar = Fn(List(Int)) :> String")))
check("an ordinary statement does not", nothing_added(typed("const x = 1")))
check("a one-liner that already closed does not",
  nothing_added(typed("func inline() 1 end")))

out("  nesting")
start_with({ "func outer()" }, 1)
split_after(1)
vim.api.nvim_buf_set_lines(0, 1, 2, false, { "  func inner()" })
vim.api.nvim_win_set_cursor(0, { 2, #"  func inner()" })
split_after(2)
lines = contents()
check("an inner block closes at its own indent",
  lines[3] == "    " and lines[4] == "  end" and lines[5] == "end", joined(lines))

out("  an existing end is not duplicated")
start_with({ "func already()", "  1", "end" }, 1)
split_after(1)
lines = contents()
check("splitting inside a closed block adds nothing",
  #lines == 4 and lines[4] == "end", joined(lines))

out("  the result is well formed")
start_with({ "func add(a: Int, b: Int) :> Int" }, 1)
split_after(1)
vim.api.nvim_buf_set_lines(0, 1, 2, false, { "  a" })
lines = contents()
check("a typed-out function reads correctly",
  lines[1] == "func add(a: Int, b: Int) :> Int"
  and lines[2] == "  a"
  and lines[3] == "end",
  joined(lines))

out("  comment highlighting")

--- The highlight groups on `row`, with runs collapsed.
local function groups(row)
  local line = vim.api.nvim_buf_get_lines(0, row - 1, row, false)[1] or ""
  local seen, last = {}, nil
  for col = 1, math.max(#line, 1) do
    local name = vim.fn.synIDattr(vim.fn.synIDtrans(vim.fn.synID(row, col, 1)), "name")
    if name ~= last then
      seen[#seen + 1] = name == "" and "-" or name
      last = name
    end
  end
  return table.concat(seen, ",")
end

local function all_comment(row)
  return groups(row) == "Comment"
end

fresh()
vim.api.nvim_buf_set_lines(0, 0, -1, false, {
  "Cmt a single line",
  "",
  "Cmt {",
  "  body",
  "}",
  "",
  "Cmt {",
  "  holds a `} escaped brace",
  "}",
  "",
  "const after = 1",
})
vim.cmd("syntax sync fromstart")

check("a single-line comment", all_comment(1), groups(1))
check("the opening brace", all_comment(3), groups(3))
check("the body", all_comment(4), groups(4))
-- The bug this covers: a plain `.*$` rule also matched `Cmt {`, and winning at
-- that column left the body and the closing brace outside the comment.
check("the closing brace", all_comment(5), groups(5))
check("a brace escaped by a backtick does not close it", all_comment(8), groups(8))
check("and the real closing brace still does", all_comment(9), groups(9))
check("code after the comment is code", groups(11) ~= "Comment", groups(11))

if failures == 0 then
  out("\nediting suite passed")
  vim.cmd("qall!")
else
  out("\n" .. failures .. " failure(s) in the editing suite")
  vim.cmd("cquit 1")
end
