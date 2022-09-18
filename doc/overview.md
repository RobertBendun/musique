## Korzystanie z interpretera w systemie GNU/Linux

Uruchamiać interpreter można na dwa sposoby:

- w trybie interaktywnym poprzez polecenie `musique`
- w trybie wsadowym poprzez podanie nazw plików do programu musique `musique utwór_a.mq utwór_b.mq`

Pliki zawierające kod Musique mają według konwencji rozszerzenie `mq`.

Tryb interaktywny jak i wsadowy nie różnią się swoimi możliwościami. Można połączyć ich działanie poprzez podanie parametru `--repl` w trakcie uruchamiania interpretera.

Aby opuścić interpreter należy użyć skrótu klawiszowego `CTRL-D`, `CTRL-C` lub polecenia `:quit`.

Interpreter posiada historię poprzednich poleceń, którą można przechodzić przy pomocy strzałek.

### Przykłady uruchamiania i korzystania z interpretera

Uruchom wyłącznie tryb interaktywny:

```console
$ musique
> play c e g
```

Uruchom plik `examples/ode-to-joy.mq`, który odgrywa "Odę do radości", grając go na porcie 14 MIDI:

```console
$ musique --output 14 examples/ode-to-joy.mq
```

by wylistować porty użyj:

```console
$ musique --list
```

Uruchom plik `examples/factorial.mq`, który wypisuje kolejne wartości silni, ale też tryb interaktywny, w którym wykorzystamy definicje z pliku:

```
$ musique examples/factorial.mq --interactive
> factorial 5
120
```

Możesz zbadać listę parametrów oferowanych przez `musique` poprzez `musique -h`

## Podstawowe koncepty

### Komentarze

Komentarze pozwalają umieszczać opisy i _komentarze_ składające się z języka naturalnego wewnątrz kodu Musique. Są dwa rodzaje komentarzy: liniowe oraz wieloliniowe.

Komentarze liniowe rozpoczynają się od znaków `--` lub `#!` i kończą się wraz z zakończeniem linii.

Komentarze wieloliniowe rozpoczynają się od co najmniej trzech znaków `-` i kończą się na co najmniej 3 znakach `-`. Pozwala to na następujące kształty komentarzy:

```
-- komentarz do końca linii
say 42; -- komentarz do końca linii, wyrażenia przed nim są wykonywane

--- komentarz wieloliniowy
say 43 <- to nie zostanie wykonane
---

say 44;

----------------------------------------------
A tutaj mamy wieloliniowy komentarz, który
ma ładne obramowania
----------------------------------------------
```

### Wartości w języku

Musique jest językiem dynamicznie typowanym - zmienne nie mają typu, wyłącznie wartości je mają.

__Typ__ jest to rodzaj wartości, determinujący w jaki sposób można je transformować, wyświetlać, grać czy w inny sposób przetwarzać.

Musique posiada kilka typów: _nil_, _bool_, _number_, _symbol_, _intrinsic_, _block_, _array_, _music_. Typ _nil_ ma pojedyńczą wartość, __nil__, która istnieje dla odróżnienia wartości od wszystkich innych wartości wartości oraz reprezentowania braku użytecznej wartości.

Typ logiczny _bool_, posiada dwie wartości: __true__ i __false__, reprezentujące odpowiednio prawdę i fałsz. __false__ nie jest jedyną metodą reprezentowania wartości fałszywej; w kontekście _warunku_ __0__, __false__, __nil__ są widziane jako fałszywe, stąd nazywane są wartościami fałszywymi (_false values_).

Typ liczbowy _number_ reprezentuje liczby wymierne, których licznik i mianownik mogą być 64-bitowymi liczbami całkowitymi. Sprawia to, że możliwym jest precyzyjne definiowanie ułamków oznaczających długość dżwięku jak `1/3` lub `3`.

Typ _symbol_ reprezentuje identyfikatory jak i stałe nazwane wewnątrz języka.

Typ funkcji wbudowanych _intrinsic_ jest używany do przechowywania funkcji napisanych wewnątrz interpretera, jak `play`, `note_on`, `par`, itp.

Typ blokowy _block_ reprezentuje kolekcję wartości, opcjonalnie sparemetryzowaną. Inaczej mówiąc blok, reprezentuje zarówno funkcje jak i tablice w Musique. Co ważne ewaluacja wnętrza bloku jest _leniwe_ - dopóki blok nie zostanie wywołany lub jego pole zindekowane, nie zostanie jego wnętrze wykonane. Blok posiadający parametry nie może zostać zindeksowany, gdyż jego wnętrze ulegnie zmianom w zależności od podanych parametrów.

Typ tablicowy _array_ reprezentuje kolekcję wartości, która w przeciwieństwie do bloków nie jest leniwa - w momencie tworzenia wszystkie wartości są już znane. Stosowana jest w celu przechowywania wyników transformacji bloków, gdyż transformowanie bloku oblicza jego wartości.

Typ muzyczny _music_ reprezentuje akord (w szczególności składający się z jednego dźwięku) w skali oferowanej przez MIDI. Najmniejszą jednostką jest półton, pojedyńczy dźwięk jest reprezentowany jako trójka (baza, opcjonalna oktawa, opcjonalna długość). Dźwięk w momencie grania może dostać domyślną wartość długości czy oktawy. Można także własnoręcznie ustawić oktawę i długość poprzez wywołanie dźwięku `c 4 (1/4)`. Jeden akord może zawierać wiele dźwięków o różnych długościach grania.

Funkcja wbudowana `typeof` wypisuje symbol reprezentujący dany typ.

### Odśmiecanie pamięci (garbage collection)

Język korzysta z automatycznego mechanizmu zarzadzania pamięcią. Sprawia to, że osoba korzystająca z pamięci nie musi przejmować się alokacją pamięci. Aktualny sposób zarzadzania pamięcią opiera się na wbudowanym w bibliotekę standardową języka C++ mechanizmu zliczania referencji. Nie jest on widziany jako ostateczny, a jako prototypowy. Może prowadzić to do nieoczekiwanego zachowania interpretera, objawiającego się wolnym działaniem i nadmiernym użytkowaniem pamięci.

## Język

Ta sekcja omawia składnię ("wygląd") oraz semantykę ("zachowanie") języka Musique. Jest to omówienie jakie programy są prawidłowe i co one znaczą.

### Składnia języka oraz literały

Kod języka Musique jest zakodowany jako UTF-8.

Musique jest językiem "free-form" - białe znaki są używane tylko do odróżnienia kolejnych słów i znaków języka, ale nie determinują jego znaczenia. Musique rozpoznaje wyłącznie białe znaki ASCII jako separatory - znaki jak niepodzielna spacja nie są rozpatrywane jako separatory.

Nazwy (identifikatory, _identifiers_) są dowolną sekwencją liter (w tym litery polskie; widzianych przez Unicode jako litery), liczb oraz znaków: `_'#$@`. Nie mogą rozpoczynać się cyfrą, nie mogą być literałem muzycznym lub zarezerwowanym identyfikatorem.

Następujące identyfikatory są zarezerwowane: `and` oraz `or`.

Musique jest językiem zwracającym uwagę na wysokość liter:

- `and` jest zarezerwowane, ale `And` nie jest.
- `c` jest literałem muzycznym, `C` może być identyfikatorem

#### Stałe liczbowe

Stała liczbowa (literał liczbowy) jest to ciąg cyfr, z potencjalnym ciągiem cyfr oznaczający część dziesiętną poprzedzony kropką.

Przykłady stałych liczbowych:

- `10.25` reprezentuje wartość `10` i `1/4`,
- `0` reprezentuje wartość `0`,
- `0.0` reprezentuje wartość `0`.

Musique posiada tylko jeden sposób sposób reprezentacji liczb dlatego `0` jest tym samym co `0.0`.

Musique __nie__ posiada stałych innych niż dziesiętne, także szesnastkowych (heksadecymalnych) czy ósemkowych.

#### Stałe muzyczne

Stałe muzyczne (literały muzyczne) jest to nazwa dźwięku z opcjonalnym przyrostkiem będącym ciągiem cyfr.


| Nazwa dźwięku | Wartość bazowa | Numer MIDI w oktawie 4 |
| :-: | :-: | :-: |
| __c__ | 0  | 60 |
| __d__ | 2  | 62 |
| __e__ | 4  | 64 |
| __f__ | 5  | 65 |
| __g__ | 7  | 67 |
| __a__ | 9  | 69 |
| __b__ | 11 | 71 |

Numer dźwięku może być modyfikowany o półton poprzez dodanie po nazwie dźwięku (co najmniej jednego):

- `#` lub `s` zwiększa o jeden półton numer dźwięku
- `b` lub `f` zmniejsza o jeden półton numer dźwięku

Numer dźwięku może być ujemny - może sprawić, że dźwięk "spadnie" do niższej oktawy.

Po nazwie dźwięku można przy pomocy liczb określić akord. Każda kolejna cyfra oznacza kolejny dźwięk w ramach akordu, liczony bezwględnie od dźwięku bazowego jako przesunięcie o daną liczbę półtonów. `c47` określa akord składający się z dźwięków `c`, `c+4`, `c+7` - akord C-dur.

### Zmienne

Zmienne są to miejsca przechowywania wartości.

Nazwa, określona przez symbol, reprezentuje odwołanie do stworzonej wcześniej zmiennej. Nie można używać zmiennych, które nie zostały wcześniej _zadeklarowane_.

Deklaracja zmiennych sprawia, że zmienna istnieje. Równocześnie z jej tworzeniem, można przypisać jej wartość.

```
x := 10;
y := 20;

say x y; -- wypisuje 10 20

t := x;
x = y;
y = t;

say x y; -- wypisuje 20 10
```

Zmienne mają zasięg leksykalny - mają dostęp tylko do zmiennych z kontekstu ich tworzenia, a nie np. z miejsca gdzie blok został wywołany.

### Wyrażenia

Musique wspiera następujące typy wyrażeń:

- sekwencję wyrażeń jak `foo; bar; foo hello`
- wyrażenia blokowe jak `[10;20;30]`
- wywołania funkcji jak `call_me maybe`
- binarne operacje jak: `10 + 20`

### Bloki

Są sekwencją wyrażeń. Każde kolejne wyrażenie oddzielone jest znakiem średnika `;`. Dla wygody, można stosować więcej niż jeden średnik do oddzielania wyrażeń (przypadkowe wciśnięcia nie są karane). Każy blok otoczony jest parą nawiasów kwadratowych `[]`, które determinują zasięg widoczności zadeklarowanych w nim zmiennych, w tym parametrów.

Blok może rozpoczynać się sekcją parametrów, która może zawierać wyłącznie symbole, a kończy się znakiem `|`. W momencie wywoływania bloku, każdemu parametrowi odpowiadać powinień argument, który nada parametrowi wartość. Literały muzyczne jak `c` __nie mogą__ zostać użyte jako parametry bloku.

Przykłady zastosowania bloków:

```
-- Blok, który nie przyjmuje żadnych parametrów i nie posiada żadnych wartości
[];

-- Blok, który nie przyjmuje żadnych parametrów, ale posiada wartość
x := [10];
say x.0; -- wypisuje 10
say (call x); -- wypisuje 10

-- Blok, który nie przyjmuje żadnych parametrów, ale posiada wiele wartości
x := [10; 20; 30];
say x.0; -- wypisuje 10
say (call x); -- wypisuje 30

-- Blok, który przyjmuje parametry i zwraca ich sumę
add := [x y | x + y ];
say (add 101 22); -- wypisuje 123

-- Blok, który odwołuje się do samego siebie
count_down := [n| say n; if (n >= 0) [ count_down (n-1) ] ];
count_down 10; -- wypisuje kolejne wartości od 10 do 0 włącznie
```

### Operacje binarne

#### Operacje logiczne

Musique wspera sume logiczną `or` oraz iloczyn logiczny `and`. Ponadto dostępne są operatory równości `==` oraz `!=`.

#### Operacje liczbowe

Musique wspiera standardowe operacje arytmetyczne (`+`, `-`, `*`, `/` oraz potęga '**') oraz porównania (`<`, `<=`, `==`, `!=`, `>=`, `>`).

#### Operacje na wartościach muzycznych

Wspierane są operatory porównania `<`, `<=`, `==`, `!=`, `>=`, `>`. Porównywać można wartości tylko o tym samym stopniu wypełnienia informacji - jeśli jedna z wartości nie ma wyspecjalizowanej podwartości jak oktawy czy długości nie można ich nawzajem porównać.

- __Dodawanie półtonów__ `a + b` gdzie `a` jest wartością muzyczną (liczbową), a `b` jest wartością liczbową (muzyczną).
- __Odejmowanie półtonów__ `a - b` gdzie `a` jest wartością muzyczną (liczbową), a `b` jest wartością liczbową (muzyczną).

Wartości muzyczne i liczbowe można zestawiać koło siebie:

- `c 1 2` tworzy `c` w oktawie 1 o długości 2
- `c c` tworzy sekwencję dźwięków `[c;c]`
- `c e g` tworzy sekwencję dźwięków `[c;e;g]`
- `c 4 e 5 g 4` tworzy sekwencję dźwięków: c w oktawie 4, e w oktawie 5 i g w oktawie 4

## Funkcje wbudowane

### Funkcje muzyczne

`par A B...` pozwala na zagranie wartości muzycznej `A` przez całą długość odtwarzania częsci `B`.

`sim a b` gra równocześnie część `A` i `B`.

`play A...` odtwarza sekwenencyjne kolejne grupy dźwięków.

Gdy funkcje `par` lub `play` dostaną tablicę wartości muzycznych tablice zostaną odtworzone sekwencyjnie. Inaczej mówiąc `play a [c;e;d] b ≡ play a c e d b`.

#### Modyfikacja kontekstu

`bpm A` ustawia długość `A` odpowiadającą ćwierćnucie w uderzeniach na minutę.

`len A` ustawia domyślną długość nuty jako `A`

`oct A` ustawia domyślną oktawę

### Funkcje sterowania przepływem

`if <condition> <then> <else>` wykonuje blok `then` jeśli `condition` jest rozpatrywane jako prawdziwe. Jeśli blok `<else>` zostanie dostarczony, to zostanie wywołany dla warunku fałszywego.

```
if true [say 42] [say 10]   -- wyświetla 42
if false [say 42] [say 10] -- wyświetla 10
say (if false [42] [10])   -- wyświetla 10
say (if false [42])        -- wyświetla nil
```

### Funkcje matematyczne

`max` znajduje maksimum z podanych wartości, a `min` znajdume minimum. W momencie podania tablic, znajduje maksimum / minumum z danej tablicy i pozostałych wartości przekazanych do funkcji.

__Wartości muzyczne, logiczne i liczbowe__ są widziane jako porównywalne tylko gdy mają ten sam typ: `c < d`, `0 < 1`, `false < true` itp.

### Funkcje tablicowe

`permute` generuje kolejną permutację tablicy.

`reverse` zwraca odwróconą tablicę.

`sort` zwraca tablicę z posortowanymi elementami

`partition` dzieli podane wartości wg predykatu, tak, że zwraca dwuelementową tablicę, w którym pierwszym elementem jest tablica wartości dla których predykat zwrócił prawdę, a drugim elementem  jest tablica wartości dla których predykat zwrócił fałsz.

```
say (partition [i | i > 5] 1 2 3 4 5 [1; 2; 3; 8] 9 0)
-- wypisuje: [[8; 9]; [1; 2; 3; 4; 5; 1; 2; 3; 0]]
```

`shuffle` zwraca tablicę z losową kolejnością elementów.

`flat` spłaszcza przekazane tablice (o jeden rząd)

```
say (flat 1 2 3);
-- wypisuje [1;2;3]
say (flat [1;2;3]);
-- wypisuje [1;2;3]
say (flat [1;2;3] 4 [5;6]);
-- wypisuje [1;2;3;4;5;6]
```

### Funkcje MIDI

`note_on <kanał> <nuta> <siła>` wysyła komunikat _Note on_.

`note_off <kanał> <nuta>` wysyła komunikat _Note off_.

`pgmchange` (znane także jako `instrument`) wysyła komunikat _Program Change_.

