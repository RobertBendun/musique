# Lista wbudowanych funkcji języka Musique

* `bmp value` – zmienia wartość BMP z domyślnej na `value`;
	-`value` musi być liczbą całkowitą, domyślnie `120`;

* `ceil value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej mniejszej lub równej tej liczbie);
	- `value` – musi być to zmienna o typie Number lub lista takich zmiennych;

* `chord (notes)` – konstruuje akord z `notes`:
	- `notes` – `notes` definiowane są następująco: `(<litera_nuty> <numer_oktawy> <czas_trwania>)`:
		- np. `(c 4 1)` – dźwięk C w 4 oktawie, o długości całej nuty;

* `down value` – sekwencyjnie zwraca liczby całkowite, począwszy od `value` do 0:
	- `value` – musi być liczbą całkowitą;

* `flat args` – łączy `args` w listę bez zagnieżdżeń (tzn. "odpakowuje" zawartość zagnieżdżonych list i zawiera je w pojedyńczej tabeli):
	- `args` - lista, w tym lista z zagnieżdżeniami;

* `floor value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej większej lub równej tej liczbie);
	- `value` – musi być to zmienna o typie Number lub lista takich zmiennych;

* `for vect` – iteruje po elementach wektora `vect`:
	- `vect` - kontener wartości, musi posiadać typ Vector;

* `hash vect` – standardowa funkcja haszująca, zwraca jeden hash połączonych wartości z `vect`: 
	- `vect` - kontener wartości, mogą być dowolnego typu;
* `if cond [if_false] [if_true]` – wyrażenie warunkowe: jeżeli `cond` będzie fałszem, zostanie wykonany kod z `[if_false]`, w przeciwnym wypadku wykonany zostanie kod z `[if_true]`;

* `incoming args` – pozwala na rozpatrzenie przychodzących komunikatów MIDI (`note_on` i `note_off`), odpowiednio;
	- `args` – konstrukcja `(komunikat, nuta)`;

* `instrument args` – pozwala na zmianę instrumentu:
	- `args` – może przyjmować sam numer programu, lub parę `(numer_programu, kanał)`;

* `len args` – zwraca długość kontenera `args`, a jeżeli `args` nie jest indeksowalne zwraca długość trwania dźwięku;

* `max args` – zwraca maksimum z `args`;

* `min args` – zwraca minimum z `args`;

* `mix args` – #TODO

* `note_off args` – w zależności od kształtu `args`:
	- jeżeli `args` są w postaci `(kanał, nuta)` – wyłącza nutę na danym kanale:
		- `kanał` – liczba całkowita;
		- `nuta` – postać podobna do `notes` z `chord notes`;
	- jeżeli `args` są w postaci `(kanał, akord)` – wyłącza wszystkie nuty z danego akordu na danym kanale;

* `note_on args` – analogicznie do `note_off args`;

* `nprimes value` – generuje `value` kolejnych liczb pierwszych:
	- `value` – musi być typu Number;

* `oct value` – podobnie do `bpm value`;

* `par args` – gra współbieżnie pierwszy dźwięk z `args` z pozostałymi dźwiękami z `args`:
	- `args` – postać `(note, ...)`, powinien być rozmiaru co najmniej 2:
		- `note` – postać podobna do `notes` z `chords notes`;

* `partition args` – dzieli `args` na dwie grupy wedle danej funkcji:
	- `args` – powinno przyjąć formę `(funkcja, lista())`

* `permute args` – permutuje `args`:
	- `args` – lista obiektów;
