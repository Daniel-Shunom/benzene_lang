-- Neovim 0.11+ LSP configuration for Benzene.
--
-- Picked up automatically by `vim.lsp.enable("benzene")`, which
-- plugin/benzene.lua calls.
--
-- Everything here can be turned off before the plugin loads:
--   vim.g.benzene_lsp_cmd      command list to launch the server
--   vim.g.benzene_compiler     path to the `ether` binary
--   vim.g.benzene_inlay_hints  true to show inferred types inline (default: off)
--   vim.g.benzene_folding      false to leave 'foldexpr' alone
--   vim.g.benzene_highlight    false to stop highlighting the word under the cursor

-- This file lives at <repo>/lsp/editors/nvim/lsp/benzene.lua, so the repository
-- root is five directories up. Resolving it this way means the config works
-- from a clone in any location, with no paths to edit.
local this_file = debug.getinfo(1, "S").source:sub(2)
local repo_root = vim.fs.normalize(vim.fn.fnamemodify(this_file, ":p:h:h:h:h:h"))

local function default_cmd()
  local launcher = repo_root .. "/lsp/bin/ether-lsp"
  if vim.fn.has("win32") == 1 then
    launcher = launcher .. ".cmd"
  end
  return { launcher }
end

--- Keep inline types opt-in; hover remains available without visual clutter.
local function enable_inlay_hints(bufnr)
  pcall(vim.lsp.inlay_hint.enable, vim.g.benzene_inlay_hints == true, { bufnr = bufnr })
end

--- Folding driven by the server's token pairing rather than by indentation.
--- `foldlevel` is left high so attaching never folds anything on its own; it
--- only makes `za` and friends work.
local function enable_folding(bufnr)
  if vim.g.benzene_folding == false or vim.lsp.foldexpr == nil then
    return
  end
  vim.api.nvim_buf_call(bufnr, function()
    vim.opt_local.foldmethod = "expr"
    vim.opt_local.foldexpr = "v:lua.vim.lsp.foldexpr()"
    vim.opt_local.foldlevel = 99
  end)
end

--- Underlines the other occurrences of whatever the cursor is resting on.
--- Neovim does not do this by itself; it only exposes the request.
local function enable_highlight(client, bufnr)
  if vim.g.benzene_highlight == false then
    return
  end
  if not client:supports_method("textDocument/documentHighlight") then
    return
  end

  local group =
    vim.api.nvim_create_augroup("benzene_highlight_" .. bufnr, { clear = true })

  vim.api.nvim_create_autocmd({ "CursorHold", "CursorHoldI" }, {
    group = group,
    buffer = bufnr,
    callback = vim.lsp.buf.document_highlight,
  })

  vim.api.nvim_create_autocmd({ "CursorMoved", "CursorMovedI" }, {
    group = group,
    buffer = bufnr,
    callback = vim.lsp.buf.clear_references,
  })

  -- The autocmds are buffer-local, but the group outlives the buffer unless it
  -- is cleaned up explicitly.
  vim.api.nvim_create_autocmd("LspDetach", {
    group = group,
    buffer = bufnr,
    callback = function()
      pcall(vim.api.nvim_del_augroup_by_id, group)
    end,
  })
end

return {
  cmd = vim.g.benzene_lsp_cmd or default_cmd(),
  filetypes = { "benzene" },

  -- Benzene has no project manifest yet, so a git checkout is the only real
  -- marker. Without one Neovim falls back to the file's own directory, which
  -- is the right behaviour for a loose `.bz` file.
  root_markers = { ".git" },

  -- The server shells out to the compiler for every analysis, so it needs to
  -- know where that binary is. The launcher scripts already default this to
  -- the compiler built from this checkout; setting it here lets a user point
  -- at a different build without editing the scripts.
  cmd_env = vim.g.benzene_compiler and { ETHER_BIN = vim.g.benzene_compiler }
    or nil,

  on_attach = function(client, bufnr)
    enable_inlay_hints(bufnr)
    enable_folding(bufnr)
    enable_highlight(client, bufnr)
  end,
}
