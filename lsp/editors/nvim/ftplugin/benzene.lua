-- Buffer-local defaults for Benzene.

-- `Cmt` is the comment introducer, which is a keyword rather than punctuation,
-- so commentstring carries the trailing space that the lexer expects.
vim.bo.commentstring = "Cmt %s"
vim.bo.comments = ":Cmt"

-- Blocks close with `end`, so two spaces is the convention the samples use.
vim.bo.expandtab = true
vim.bo.shiftwidth = 2
vim.bo.softtabstop = 2

local bufnr = vim.api.nvim_get_current_buf()

-- Setting the filetype again would otherwise attach a second watcher, and two
-- watchers write two `end`s.
if vim.g.benzene_auto_end == false or vim.b[bufnr].benzene_auto_end_attached then
  return
end
vim.b[bufnr].benzene_auto_end_attached = true

local blocks = require("benzene.blocks")

--- Closes the block that was just opened, if one was.
local function close_block()
  if not vim.api.nvim_buf_is_valid(bufnr) then
    return
  end

  local row = vim.api.nvim_win_get_cursor(0)[1]
  if row < 2 then
    return
  end

  local lines = vim.api.nvim_buf_get_lines(bufnr, 0, -1, false)
  local current = lines[row]
  local opener = lines[row - 1]

  -- Only a line that was just opened below a block header, and only while it
  -- is still empty: anything typed on it means the moment has passed.
  if current == nil or current:match("^%s*$") == nil then
    return
  end
  if not blocks.opens_block(opener) then
    return
  end

  local indent = blocks.indent_of(opener)
  if blocks.already_closed(lines, row - 1, indent) then
    return
  end

  vim.api.nvim_buf_set_lines(bufnr, row, row, false, { indent .. "end" })

  -- Put the cursor inside the block. Only when nothing has indented the line
  -- already, so an indent plugin or a deliberate indent wins.
  if #blocks.indent_of(current) <= #indent then
    local body = indent .. string.rep(" ", vim.bo[bufnr].shiftwidth)
    vim.api.nvim_buf_set_lines(bufnr, row - 1, row, false, { body })
    vim.api.nvim_win_set_cursor(0, { row, #body })
  end
end

-- Watching the buffer rather than mapping a key.
--
-- `<CR>` would be the precise trigger, but a buffer-local mapping shadows the
-- global one, and in any configuration with a completion plugin that is the
-- mapping which accepts a completion. The cost of being precise here would be
-- breaking Enter in exactly the buffers this is meant to help.
vim.api.nvim_buf_attach(bufnr, false, {
  on_lines = function(_, _, _, _, last_old, last_new)
    -- Exactly one line appeared: a split, not a paste or an undo.
    if last_new - last_old ~= 1 then
      return
    end

    -- The buffer cannot be edited from inside this callback, and scheduling is
    -- also what lets the insertion land after the split has settled.
    vim.schedule(function()
      pcall(close_block)
    end)
  end,
})
