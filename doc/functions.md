# Lista wbudowanych funkcji języka Musique

* `bmp value` – zmienia wartość BMP z domyślnej na `value`;
	-`value` musi być liczbą całkowitą, domyślnie `120`;

* `call function args` – funkcja wywołująca funkcję `function`, która przekazuje `function` `args` jako argumenty wywoływanej fukncji;

* `ceil value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej mniejszej lub równej tej liczbie);
	- `value` – musi być to wartość o typie Number lub tablica takich wartości;

* `chord (notes)` – konstruuje akord z `notes`:
	- `notes` – `notes` definiowane są następująco: `(<litera_nuty> <numer_oktawy> <czas_trwania>)`:
		- np. `(c 4 1)` – dźwięk C w 4 oktawie, o długości całej nuty;

* `down value` – sekwencyjnie zwraca liczby całkowite, począwszy od `value` do 0:
	- `value` – musi być liczbą całkowitą;

* `flat args` – łączy `args` w tablicę bez zagnieżdżeń (tzn. "odpakowuje" zawartość zagnieżdżonych tablic i zawiera je w pojedyńczej tabeli):
	- `args` - tablica, w tym tablica z zagnieżdżeniami;

* `floor value` – operacja podobna do matematycznej funkcji podłogi (zaokrąglenie liczby do pierwszej liczby całkowitej większej lub równej tej liczbie);
	- `value` – musi być to zmienna o typie Number lub tablica takich zmiennych;

* `fold args` – używa elementów tablicy jako argumentów podanej funkcji:
	- `args` – postaci `tablica funkcja` lub `tablica wartość_startowa funkcja`;

* `for vect` – iteruje po elementach wektora `vect`:
	- `vect` - kontener wartości, musi posiadać typ Vector;

* `hash vect` – standardowa funkcja haszująca, zwraca jeden hash połączonych wartości z `vect`: 
	- `vect` - kontener wartości, mogą być dowolnego typu;

* `if cond [if_true] [if_false]` – wyrażenie warunkowe: jeżeli `cond` będzie prawdą, zostanie wykonany kod z `[if_true]`, w przeciwnym wypadku wykonany zostanie kod z `[if_false]` – fragment `[if_false]` jest opcjonalny;

* `incoming args` – pozwala na rozpatrzenie przychodzących komunikatów MIDI (`note_on` i `note_off`), odpowiednio;
	- `args` – konstrukcja `komunikat, nuta`;

* `instrument args` – pozwala na zmianę instrumentu:
	- `args` – może przyjmować sam numer programu, lub parę `numer_programu, kanał`;

* `len args` – zwraca długość kontenera `args`, a jeżeli `args` nie jest wektorem ustawia domyślną długość trwania dźwięku, domyślnie ćwierćnuta;

* `max args` – zwraca maksimum z `args`;

* `min args` – zwraca minimum z `args`;

* `mix args` – algorytmicznie miesza wszystkie elementy z `args`:
	- `args` – tablica elementów, może być tablicą z zagnieżdżeniami;

* `note_off args` – w zależności od kształtu `args`:
	- jeżeli `args` są w postaci `(kanał, nuta)` – wyłącza nutę na danym kanale:
		- `kanał` – liczba całkowita;
		- `nuta` – postać podobna do `notes` z `chord notes`;
	- jeżeli `args` są w postaci `(kanał, akord)` – wyłącza wszystkie nuty z danego akordu na danym kanale;

* `note_on args` – analogicznie do `note_off args`;

* `nprimes value` – generuje `value` kolejnych liczb pierwszych:
	- `value` – musi być typu Number;

* `oct value` – analogicznie do `bpm value`, wartość domyślna to 4;

* `par args` – gra współbieżnie pierwszy dźwięk z `args` z pozostałymi dźwiękami z `args`:
	- `args` – postać `(note, ...)`, powinien być rozmiaru co najmniej 2:
		- `note` – postać podobna do `notes` z `chords notes`;

* `partition args` – dzieli `args` na dwie grupy wedle danej funkcji:
	- `args` – powinno przyjąć formę `(funkcja, tablica())`

* `permute args` – permutuje `args`:
	- `args` – tablica obiektów;

* `pgmchange args` – analogicznie do `instrument args`;

* `play args` – gra `args`:
	- `args` – mogą być to pojedyncze nuty, tablica nut oraz bloki kodu (nuty analogicznie jak w `chord notes`);

* `program_change args` – analogicznie do `instrument args`;

* `range args` – zwraca tablicę wartości liczbowych w podanych w `args` zakresie:
	- `args` – postać `stop`, `start stop` lub `start stop step`;

* `reverse args` – odwraca kolejność elementów `args`;
	- `args` – powinna być to tablica;

* `rotate args` – przenosi na koniec tablicy wskazaną ilość elementów:
	- `args` – musi być postaci `liczba tablica`;

* `round value` – zaokrągla wartość zgodnie z reguałmi matematyki:
	- `value` – musi być to wartość liczbowa;

* `shuffle args` – tasuje elementy `args`:
	- `args` – powinna być to tablica;

* `sort args` – sortuje elementy `args`:
	- `args` – powinna być to tablica;

* `try args` – próbuje wykonać wszystkie bloki kodu poza ostatnim, a jeżeli w trakcie tej próby natrafi na błąd, wykonuje ostatni blok:
	- `args` – musi być to co najmniej jeden blok kodu Musique;

* `typeof variable` – zwraca typ wskazanej `variable`;

* `uniq args` – zwraca tablicę elementów, z której usunięto następujące po sobie powtórzenia:
	- `args` – powinna być to tablica;

* `unique args` – zwraca tablicę elementów, z której usunięto powtórzenia:
	- `args` – powinna być to tablica;

* `up value` – analogicznie do `down value`;

* `update args` – aktualizuje element tablicy do nowej wartości:
	- `args` – postaci `tablica indeks wartość`;

