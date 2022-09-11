# Lista wbudowanych funkcji języka Musique

* `bmp value` – zmienia wartość BMP z domyślnej na `value`;
	-`value` musi być liczbą całkowitą, domyślnie `120`;

* `ceil value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej mniejszej lub równej tej liczbie);
	- `value` – musi być to wartość o typie Number lub lista takich wartości;

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

* `if cond [if_true] [if_false]` – wyrażenie warunkowe: jeżeli `cond` będzie prawdą, zostanie wykonany kod z `[if_true]`, w przeciwnym wypadku wykonany zostanie kod z `[if_false]` – fragment `[if_false]` jest opcjonalny;

* `incoming args` – pozwala na rozpatrzenie przychodzących komunikatów MIDI (`note_on` i `note_off`), odpowiednio;
	- `args` – konstrukcja `komunikat, nuta`;

* `instrument args` – pozwala na zmianę instrumentu:
	- `args` – może przyjmować sam numer programu, lub parę `numer_programu, kanał`;

* `len args` – zwraca długość kontenera `args`, a jeżeli `args` nie jest indeksowalne zwraca długość trwania dźwięku;

* `max args` – zwraca maksimum z `args`;

* `min args` – zwraca minimum z `args`;

* `mix args` – algorytmicznie miesza wszystkie elementy z `args`:
	- `args` – lista elementów, może być listą z zagnieżdżeniami;

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

* `pgmchange args` – analogicznie do `instrument args`;

* `play args` – gra `args`:
	- `args` – mogą być to pojedyncze nuty, listy nut oraz bloki kodu zwracające nuty (nuty analogicznie jak w `chord notes`);

* `program_change args` – analogicznie do `instrument args`;

* `range args` – zwraca listę wartości liczbowych w podanych w `args` zakresie:
	- `args` – postać `stop`, `start stop` lub `start stop step`;

* `reverse args` – odwraca kolejność elementów `args`;
	- `args` – powinna być to lista;

* `rotate args` – przenosi na koniec listy wskazaną ilość elementów:
	- `args` – musi być postaci `liczba lista`;

* `round value` – zaokrągla wartość zgodnie z reguałmi matematyki:
	- `value` – musi być to wartość liczbowa;

* `shuffle args` – tasuje elementy `args`:
	- `args` – powinna być to lista;

* `sort args` – sortuje elementy `args`:
	- `args` – powinna być to lista;

* `try args` – próbuje wykonać wszystkie bloki kodu poza ostatnim, a jeżeli w trakcie tej próby natrafi na błąd, wykonuje ostatni blok:
	- `args` – musi być to co najmniej jeden blok kodu Musique;

* `typeof variable` – zwraca typ wskazanej `variable`;

* `uniq args` – zwraca elementy list, z której usunięto elementy powtórzone:
	- `args` – powinna być to lista;

* `unique args` – zwraca listę elementów, z której usunięto powtórzenia:
	- `args` – powinna być to lista;

* `up value` – analogicznie do `down value`;

* `update args` – aktualizuje element listy do nowej wartości:
	- `args` – postaci `lista indeks wartość` lub `blok indeks wartość`;

