-- Turns the Benzene language server on.
--
-- Adding this directory to Neovim's runtimepath is the whole installation: the
-- filetype, the fallback syntax and the LSP client all come along with it.

if vim.g.loaded_benzene then
  return
end
vim.g.loaded_benzene = true

-- Requires the native LSP configuration API, which landed in Neovim 0.11.
if vim.fn.has("nvim-0.11") == 0 then
  vim.notify(
    "benzene: the language server config needs Neovim 0.11 or newer",
    vim.log.levels.WARN
  )
  return
end

vim.lsp.enable("benzene")

-- Semantic tokens are what colour an identifier by what the compiler resolved
-- it to, so they are worth confirming are on. Neovim applies them
-- automatically whenever the server advertises the capability.
vim.api.nvim_create_user_command("BenzeneLspStatus", function()
  local clients = vim.lsp.get_clients({ bufnr = 0, name = "benzene" })
  if #clients == 0 then
    vim.notify(
      "benzene: no language server attached to this buffer\n"
        .. "check :LspLog, and that lsp/build.cmd (or build.sh) has been run",
      vim.log.levels.WARN
    )
    return
  end

  for _, client in ipairs(clients) do
    vim.notify(
      ("benzene: %s attached (id %d), position encoding %s"):format(
        client.name,
        client.id,
        client.offset_encoding
      )
    )
  end
end, { desc = "Report whether the Benzene language server is attached" })
