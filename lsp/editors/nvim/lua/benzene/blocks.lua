-- Recognising the constructs that close with `end`.
--
-- Deliberately textual. The language server knows the real token stream, but it
-- is always at least one keystroke behind the buffer, and the line this has to
-- reason about is the one just typed. Asking the server would mean asking about
-- text it has not seen.
--
-- Kept as pure functions over strings so they can be tested without a buffer.

local M = {}

--- Constructs that open a block terminated by `end`.
---
--- Braces are left alone: `{}` is handled by whatever autopair plugin is
--- already installed, and fighting it would produce two closers.
local OPENERS = {
  "^%s*func%s",   -- func name(...) ... end
  "^%s*case%s",   -- case expr: ... end
}

--- A lambda anywhere on the line opens a block too: `let f = Fn(x) :> Int`.
local LAMBDA = "Fn%s*%("

--- Lines that may look like openers but are not.
local EXEMPT = {
  "^%s*Cmt%s",   -- a comment runs to end of line
  "^%s*Cmt$",
  -- A type expression may contain `Fn(...)` as a type rather than a lambda:
  -- `type FooBar = Fn(List(Int)) :> String` declares a type and ends there.
  "^%s*type%s",
}

local function matches_any(line, patterns)
  for _, pattern in ipairs(patterns) do
    if line:match(pattern) then
      return true
    end
  end
  return false
end

--- The leading whitespace of a line.
function M.indent_of(line)
  return line:match("^%s*") or ""
end

--- Whether `line` starts a block that wants an `end`.
---
--- A line that already carries its own `end` is complete, so `func f() 1 end`
--- opens nothing.
function M.opens_block(line)
  if line == nil or matches_any(line, EXEMPT) then
    return false
  end

  if line:match("%f[%w]end%f[%W]") then
    return false
  end

  return matches_any(line, OPENERS) or line:match(LAMBDA) ~= nil
end

--- Whether the block opened at `index` already has its `end`.
---
--- Looks forward for the first line indented no further than the opener. Only a
--- line at exactly the opener's indent can be its `end`; one indented *less*
--- belongs to an enclosing block, and finding that means this block was never
--- closed. Treating the two the same makes a nested `func` believe the outer
--- function's `end` was its own.
---
--- This is what stops a second `end` appearing when a line is split inside a
--- function that already has one.
---
--- `lines` is 1-based, as `nvim_buf_get_lines` returns it.
function M.already_closed(lines, index, indent)
  local width = #indent

  for position = index + 1, #lines do
    local line = lines[position]
    if line:match("^%s*$") == nil then
      local this_width = #M.indent_of(line)
      if this_width < width then
        return false
      end
      if this_width == width then
        return line:match("^%s*end%f[%W]") ~= nil
      end
    end
  end

  -- Ran out of buffer without finding anything at this level: nothing closes it.
  return false
end

return M
