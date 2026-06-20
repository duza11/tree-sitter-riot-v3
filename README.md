# tree-sitter-riot-v3

Riot.js v3 `.tag` grammar for [tree-sitter](https://tree-sitter.github.io/tree-sitter/).

## Features

- Riot v3 component files with one or more top-level components.
- Template expressions in text and attributes, including braces in JavaScript strings, template literals, regular expressions, and comments.
- HTML-like normal, void, and self-closing elements.
- Unwrapped component JavaScript used by Riot v3.
- `script` raw text with JavaScript injection.
- `style` raw text with CSS injection.
- `style type="scss"` raw text with SCSS injection.
- Neovim queries under `queries/riot_v3/`.

## Neovim setup

Example for `nvim-treesitter`:

```lua
local parser_config = require("nvim-treesitter.parsers").get_parser_configs()

parser_config.riot_v3 = {
  install_info = {
    url = "/path/to/tree-sitter-riot-v3",
    files = { "src/parser.c", "src/scanner.c" },
  },
  filetype = "riot",
}

vim.filetype.add({
  extension = {
    tag = "riot",
  },
})
```

Then copy or expose this repository on `runtimepath` so Neovim can find:

```text
queries/riot_v3/highlights.scm
queries/riot_v3/injections.scm
queries/riot_v3/folds.scm
```

If your Neovim setup uses parser name as filetype directly, map `.tag` to `riot_v3` instead.

## Development

```sh
tree-sitter generate
tree-sitter test
tree-sitter parse examples/basic.tag
```
