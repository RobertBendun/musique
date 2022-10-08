# Musique interpreter

Interpreter języka Musique. Możliwy do wykorzystywania jako:

- biblioteka interpretera języka dołączana do innego projektu (podobnie jak Lua);
- REPL działający w systemie GNU/Linux + ALSA wykonujący język Musique.

## Wymagane pakiety systemowe

Do poprawnego skompilowania i uruchomienia interpretera języka Musique należy posiadać zainstalowane następujące pakiety (lub ich odpowiedniki) – dla systemu GNU/Linux Ubuntu Desktop 22.04 są to:

- `build-essential` – pakiet zawierający podstawowe narzędzia do pracy z kodem źródłowym, takie jak m.in. kompilator;
- `libasound2-dev` – pakiet zawierający biblioteki programistyczne pakietu `libasound2`.

Można je zainstalować korzystając z polecenia:

```
$ sudo apt update
$ sudo apt install -y build-essential libasound2-dev
```

## Budowanie interpretera

- Aby wybudować wersję Linux+ALSA: `make`
- Aby wybudować wersję Windows: `make os=windows`

Żeby zainstalować interpreter języka Musique w systemie, należy dodatkowo wykonać polecenie:

```
# make install
```

*Uwaga*: powyższe polecenie instalacyjne musi zostać wykonane jako uprzywilejowany użytkownik (np. wykorzystując polecenie `sudo`).

## Dostępne komendy

- `make` - Buduje interpreter `bin/musique` (tryb release)
- `make debug` - Buduje interpreter `bin/debug/musique` (tryb debug)
- `make clean` - Usuwa reprodukowalne elementy projektu (automatycznie stworzone pliki binarne czy raporty)

### Dokumentacja

- `make doc` - Tworzy `doc/build/html/` zawierający dokumentację projektu

### Testowanie

- `make test` - Uruchom wszystkie dostępne testy automatyczne

## Kolorowanie składni

### Vim / Neovim

Skopiuj plik [etc/editor/musique.vim](etc/editor/musique.vim) do folderu `syntax` wewnątrz twojej konfiguracji Vima (Neovima). Np:

```console
$ cp editor/musique.vim ~/.config/nvim/syntax/
```

Następnie musisz dodać ustawienie typu pliku na podstawie rozszerzenia wewnątrz twojej konfiguracji:

```vim
au BufRead,BufNewFile *.mq set filetype=musique
```

### Visual Studio Code

Skopiuj katalog [etc/editor/musique-vscode](etc/editor/musique-vscode) do folderu `<user home>/.vscode/extensions` i uruchom ponownie program VSCode.

# Thanks to

- Creator of [tl::expected](https://github.com/TartanLlama/expected) - [Sy Brand](https://sybrand.ink/)
- Creator of [bestline](https://github.com/jart/bestline) - [Justine Tunney](https://justinetunney.com/)
- Creator of [rtmidi](https://github.com/thestk/rtmidi/) - [Gary P. Scavone](http://www.music.mcgill.ca/~gary/)

and all contributors that created libraries above.
