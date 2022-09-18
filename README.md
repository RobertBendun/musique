# Musique interpreter

Interpreter języka Musique. Możliwy do wykorzystywania jako:

- biblioteka interpretera języka dołączana do innego projektu (podobnie jak Lua);
- REPL działający w systemie GNU/Linux + ALSA wykonujący język Musique.

## Budowanie interpretera

Jeśli nie posiadasz zależności `lib/midi` to:

```console
$ git submodule init
$ git submodule update
$ (cd lib/midi; make)
```

A następnie zbuduj interpreter:

```console
$ make bin/musique
```

## Dostępne komendy

- `make` - Buduje interpreter `bin/musique` (tryb release)
- `make debug` - Buduje interpreter `bin/debug/musique` (tryb debug)
- `make clean` - Usuwa reprodukowalne elementy projektu (automatycznie stworzone pliki binarne czy raporty)

### Dokumentacja

- `make doc` - Tworzy `doc/build/html/` zawierający dokumentację projektu

### Testowanie

- `make test` - Uruchom wszystkie dostępne testy automatyczne
- `make unit-tests` - Uruchamia testy jednostkowe interpretera
- `make unit-test-coverage` - Uruchamia raport pokrycia kodu przez testy jednostkowe
- `scripts/test.py test examples` - Uruchamia testy zachowań przykładów
- `scripts/test.py record examples` - Nagrywa testy zachowań przykładów

### Debugowanie

- `scripts/log-function-calls.sh` - Tworzy listę wywołań funkcji używając GDB

## Budowa projektu

```
.
├── bin            Miejsce produkcji plików wykonywalnych
├── coverage
├── doc            Dokumentacja języka, interpretera
│   └── build      Miejsce produkcji dokumentacji
├── editor         Pluginy do edytorów dodające wsparcie dla języka
├── lib            Zewnętrzne zależności projektu
│   ├── expected
│   └── ut
└── include        Główny katalog z plikami nagłówkowymi
├── scripts        Skrypty wspierające budowanie i tworzenie
└── src            Główny katalog z plikami źródłowymi
    └── tests      Katalog z testami jednostkowymi
```

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
