# Lista wbudowanych funkcji języka Musique

0. `bmp value` – zmienia wartość BMP z domyślnej na `value`;
	-`value` musi być liczbą całkowitą, domyślnie `120`;

1. `ceil value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej mniejszej lub równej tej liczbie);
	-`value` – musi być to zmienna o typie Number lub lista takich zmiennych;

2. `chord (notes)` – konstruuje akord z `notes`:
	- `notes` – `notes` definiowane są następująco: `(<litera_nuty> <numer_oktawy> <czas_trwania>)`:
		- np. `(c 4 1)` – dźwięk C w 4 oktawie, o długości całej nuty;

3. `down value` – sekwencyjnie zwraca liczby całkowite, począwszy od `value` do 0:
	- `value` – musi być liczbą całkowitą;

4. `flat args` – łączy `args` w listę bez zagnieżdżeń (tzn. "odpakowuje" zawartość zagnieżdżonych list i zawiera je w pojedyńczej tabeli):
	- `args` - lista, w tym lista z zagnieżdżeniami;

5. `floor value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej większej lub równej tej liczbie);
	-`value` – musi być to zmienna o typie Number lub lista takich zmiennych;

6. `for vect` – iteruje po elementach wektora `vect`:
	- `vect` - kontener wartości, musi posiadać typ Vector;

7. `hash vect` – standardowa funkcja haszująca, zwraca jeden hash połączonych wartości z `vect`: 
	- `vect` - kontener wartości, mogą być dowolnego typu;
8. `if cond [if_false] [if_true]` – wyrażenie warunkowe: jeżeli `cond` będzie fałszem, zostanie wykonany kod z `[if_false]`, w przeciwnym wypadku wykonany zostanie kod z `[if_true]`;

9. `incoming args` – pozwala na rozpatrzenie przychodzących komunikatów MIDI (`note_on` i `note_off`), odpowiednio;
	- `args` – konstrukcja `(komunikat, nuta)`;
`
