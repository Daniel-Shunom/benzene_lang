-- Buffer-local defaults for Benzene.
--
-- `Cmt` is the comment introducer, which is a keyword rather than punctuation,
-- so commentstring carries the trailing space that the lexer expects.
vim.bo.commentstring = "Cmt %s"
vim.bo.comments = ":Cmt"

-- Blocks close with `end`, so two spaces is the convention the samples use.
vim.bo.expandtab = true
vim.bo.shiftwidth = 2
vim.bo.softtabstop = 2
