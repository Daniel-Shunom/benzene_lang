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

local function clients(bufnr)
  return vim.lsp.get_clients({ bufnr = bufnr or 0, name = "benzene" })
end

vim.api.nvim_create_user_command("BenzeneLspStatus", function()
  local attached = clients()
  if #attached == 0 then
    vim.notify(
      "benzene: no language server attached to this buffer\n"
        .. "check :LspLog, and that lsp/build.cmd (or build.sh) has been run",
      vim.log.levels.WARN
    )
    return
  end

  for _, client in ipairs(attached) do
    local hints = vim.lsp.inlay_hint.is_enabled({ bufnr = 0 }) and "on" or "off"
    vim.notify(
      ("benzene: %s attached (id %d)\n  position encoding: %s\n  inlay hints: %s\n  command: %s"):format(
        client.name,
        client.id,
        client.offset_encoding,
        hints,
        table.concat(client.config.cmd or {}, " ")
      )
    )
  end
end, { desc = "Report whether the Benzene language server is attached" })

vim.api.nvim_create_user_command("BenzeneInlayHints", function()
  local shown = vim.lsp.inlay_hint.is_enabled({ bufnr = 0 })
  vim.lsp.inlay_hint.enable(not shown, { bufnr = 0 })
  vim.notify("benzene: inlay hints " .. (shown and "off" or "on"))
end, { desc = "Toggle inferred-type hints in this buffer" })

vim.api.nvim_create_user_command("BenzeneCodeActions", function()
  vim.lsp.buf.code_action()
end, { desc = "Show Benzene code actions at the cursor" })

vim.api.nvim_create_user_command("BenzeneRestart", function()
  -- Picks up a rebuilt server, which is otherwise only loaded at attach time.
  for _, client in ipairs(clients()) do
    client:stop()
  end

  vim.defer_fn(function()
    -- Re-firing FileType is what makes the enabled config attach again.
    vim.cmd("edit")
    vim.notify("benzene: language server restarted")
  end, 500)
end, { desc = "Restart the Benzene language server for this buffer" })
