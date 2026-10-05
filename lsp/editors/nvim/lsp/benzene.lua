-- Neovim 0.11+ LSP configuration for Benzene.
--
-- Picked up automatically by `vim.lsp.enable("benzene")`, which plugin/benzene.lua
-- calls. Override the launcher by setting `vim.g.benzene_lsp_cmd` to a command
-- list before the plugin loads.

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
}
