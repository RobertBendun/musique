# Musique interpreter

## Dostępne komendy

- `make` - Buduje interpreter `bin/musique`
- `make clean` - Usuwa reprodukowalne elementy projektu (automatycznie stworzone pliki binarne czy raporty)
- `make unit-test-coverage` - Uruchamia raport pokrycia kodu przez testy jednostkowe
- `make unit-tests` - Uruchamia testy jednostkowe interpretera

## Budowa projektu

```
.
├── bin            Miejsce produkcji plików wykonywalnych
├── doc            Dokumentacja języka, interpretera
├── lib            Zewnętrzne zależności projektu
│   ├── expected
│   └── ut
└── src            Główny katalog z kodem źródłowym
    └── tests      Katalog z testami jednostkowymi
```
