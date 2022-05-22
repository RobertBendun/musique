# Musique interpreter

Interpreter języka Musique. Możliwy do wykorzystywania jako:

- biblioteka interpretera języka dołączana do innego projektu (podobnie jak Lua);
- REPL działający w systemie GNU/Linux + ALSA wykonujący język Musique.

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
- `etc/tools/test.py test examples` - Uruchamia testy zachowań przykładów
- `etc/tools/test.py record examples` - Nagrywa testy zachowań przykładów

### Debugowanie

- `etc/tools/log-function-calls.sh` - Tworzy listę wywołań funkcji używając GDB

## Budowa projektu

```
.
├── bin            Miejsce produkcji plików wykonywalnych
├── coverage
├── doc            Dokumentacja języka, interpretera
│   ├── build      Miejsce produkcji dokumentacji
│   └── source     Źródła dokumentacji Sphinx
├── etc/tools      Dodatkowe narzędzia
├── lib            Zewnętrzne zależności projektu
│   ├── expected
│   └── ut
└── src            Główny katalog z kodem źródłowym
    └── tests      Katalog z testami jednostkowymi
```
