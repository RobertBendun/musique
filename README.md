# Musique interpreter

Reference implementation of Musique programming language.

## Building

Reference [`build_instructions.md`](./build_instructions.md).

## Syntax highlighting

### Vim / Neovim

Copy [editor/musique.vim](editor/musique.vim) to `syntax` directory inside your Vim (Neovim) configuration.

```console
$ cp editor/musique.vim ~/.config/nvim/syntax/
```

Next, you can add bindings for filetype:

```vim
au BufRead,BufNewFile *.mq set filetype=musique
```

### Visual Studio Code

Copy [editor/vscode-musique](editor/vscode-musique) directory to `<user home>/.vscode/extensions` and restart VS Code.

# Thanks to

- Creator of [tl::expected](https://github.com/TartanLlama/expected) - [Sy Brand](https://sybrand.ink/)
- Creator of [bestline](https://github.com/jart/bestline) - [Justine Tunney](https://justinetunney.com/)
- Creator of [rtmidi](https://github.com/thestk/rtmidi/) - [Gary P. Scavone](http://www.music.mcgill.ca/~gary/)
- Creators of [link](https://github.com/Ableton/link)

and all contributors that created libraries above.
