# Musique

[![Build and test Musique](https://github.com/RobertBendun/musique/actions/workflows/build.yml/badge.svg)](https://github.com/RobertBendun/musique/actions/workflows/build.yml)
[![Coverage Status](https://coveralls.io/repos/github/RobertBendun/musique/badge.svg?branch=main)](https://coveralls.io/github/RobertBendun/musique?branch=main)

Reference implementation of Musique programming language.

## Building

```
$ meson build
$ cd build
$ ninja
```

You can find prebuild releases [here](https://musique.students.wmi.amu.edu.pl/).

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
- Creator of [rtmidi](https://github.com/thestk/rtmidi/) - [Gary P. Scavone](http://www.music.mcgill.ca/~gary/)
- Creators of [link](https://github.com/Ableton/link)
- Creators of [Catch2](https://github.com/catchorg/Catch2/)

and all contributors that created libraries above.
