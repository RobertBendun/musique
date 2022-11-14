---
title: Wprowadzenie do języka Musique
date: 2022-11-12
author: Robert Bendun <robben@st.amu.edu.pl>
---

# Wprowadzenie do języka Musique

W dalszej części tekstu przyjmujemy następujące konwencje:

`>` – jest to [znak zachęty](https://pl.wikipedia.org/wiki/Znak_zach%C4%99ty) i wskazuje gotowość programu Musique do wprowadzania tekstu. Jeżeli widzisz taki znak, oznacza to, że jesteś w programie Musique.

Przykładowo:

```
> 3 + 4
7
>
```

W tym przykładzie wykonujemy `3 + 4` , zatwierdzając ENTERM, w zwrocie otrzymując – w tym konkretnym przypadku – wynik operacji.

Nawiasem mówiąc – przykłady w tym wprowadzeniu mogą rozpoczynać się od `>` – oznacza to, że polecenia wprowadzono w sesji interaktywnej, lub bez tego znaku – wtedy traktujemy przykład jako zapisany plik z poleceniami.
Nie ma różnic pomiędzy wykonywaniem poleceń języka Musique zawartych w pliku, a poleceniami wprowadzanymi w sesji interaktywnej – wybór pozostawiamy użytkownikowi.

Można wykonać złożone polecenia (które w pliku zostałby rozbite na kilka linii) w sesji interaktywnej - należy wtedy wpisać je w formie jednej linii.

## Nuty, pauzy, akordy

W tej sekcji zostanie omówione tworzenie prostych utworów z wykorzystaniem nut, akordów oraz prostych sekwencji.

### Nuty

Aby zagrać nutę C wpisz `play c` w uruchomionym programie Musique.

```
> play c
```

Możesz zagrać inną nutę jak E:

```
> play e
```

lub wiele nut po kolei – należy umieścić je w nawiasach okrągłych, rozdzielone przecinkami:

```
> play (c, e, g)
```

Dostępne dźwięki bazowe to kolejno: `c`, `d`, `e`, `f`, `g`, `a`, `b`. Można je modyfikować poprzez znaki:

- `#` lub `s` podwyższający wysokość dźwięku o pół tonu (**s**harp)
- `b` lub `f` obniżający wysokość dźwięku o pół tonu (**b**mol, **f**lat)

pozwala to przykładowo tworzyć:

- `c#` - dźwięk Cis
- `c##` - dźwięk D
- `eb` - dźwięk Dis

Aby dźwięk został odtworzony w wybranej oktawie można dołączyć do znaku dźwięku liczbową wartość oktawy - `c4` to dźwięk c w 4 oktawie, `c#8` to dźwięk cis w 8 oktawie, itp.

```
> play (c4, c5, c6)
```

Aby dźwięk trwał określoną długość należy podać jako dodatkowy parametr pożądaną wartość, przykładowo:

Sekwencja złożona z dźwięku C w 4 oktawie trwającym ćwierćnutę, dźwięku D w 5 oktawie trwającym półnutę oraz dźwięku E w 4 oktawie trwającym całą nutę wygląda następująco:

```
> play (c4 (1/4), d5 (1/2), e4 1)
```

Można zapis uprościć korzystając z predefiniowanych czasów dźwięków:

| nazwa | czas trwania | liczbowo |
| :-: | :-: | :-: |
| `wn` | cała nuta | `1` |
| `dwn` | cała nuta z kropką | `3/2` |
| `hn` | półnuta | `1/2` |
| `dhn` | półnuta z kropką | `3/4` |
| `qn` | ćwierćnuta | `1/4` |
| `dqn` | ćwierćnuta z kropką | `3/8` |
| `en` | ósemka | `1/8` |
| `den` | ósemka z kropką | `3/16` |
| `sn` | szesnastka | `1/16` |
| `dsn` | szesnastka z kropką | `3/32` |
| `tn` | trzydziestkadwójka | `1/32` |

Korzystając z tych długości możemy powyższy przykład zapisać równoważnie w ten sposób:

```
> play (c4 qn, d5 hn, e4 1)
```

### Pauzy

Pauzy stosuje się poprzez użycie litery `p`:

```
> play (c4 hn, p qn, c4 hn, p qn, c4 hn)
```

### Akordy

Aby odegrać akord należy umieścić symbole dźwięków, z których miałby się składać, w wywołaniu funkcji `chord`:

```
> play (chord (c, e, g))
```

W utworze możemy chcieć częściej używać tych samych akordów czy sekwencji. Przypisanie ich do wybranej nazwy wyglądałoby tak:

```
> sekwencja := (c, e, g)
> sekwencja
(c, e, g)
```

Wpisanie samej nazwy pozwala zobaczyć co się pod nią znajduje.

Zapisaną sekwencję możemy przerobić na akord, ponownie wykorzystując funkcję `chord`:

```
> C := chord sekwencja
> C
chord (c, e, g)
```

i następnie go odtworzyć:

```
> play C
```

Należy zwrócić uwagę, że Musique rozróżnia wielkość liter. Ponadto nazw będących oznaczeniami dźwięków jak `c` bądź `cffff` nie można używać jako nazw dla zmiennych.

## Metody tworzenia sekwencji

### Wielokrotne powtarzanie tego samego dźwięku

Wielokrotne powtarzanie tego samego dźwięku można osiągnąć poprzez mnożenie dźwięku przez ilość powtórzeń:

```
> 3 * c
(c, c, c)
> c * 3
(c, c, c)
> 3 * (c, e)
(c, e, c, e, c, e)
```

Operacja ta zachowuje długości i oktawy:

```
> 3 * (e4 qn, f8 tn)
(e4 1/4, f8 1/32, e4 1/4, f8 1/32, e4 1/4, f8 1/32)
```

### Wybieranie losowego dźwięku z grupy

Funkcja `pick` pozwala na wybór losowego elementu z sekwencji. Można użyć jej np. do grania dźwięku o losowej długości:

```
> play (c4 (pick qn hn))
```

W powyższym przykładzie dźwięk C w oktawie 4 zostanie zagrany losowo z długością ćwierćnuty lub półnuty.

### Losowe ustawienie elementów sekwencji

Funkcja `shuffle` pozwala na tworzenie sekwencji z losową kolejnością elementów, np:

```
> shuffle (c, e, f, g)
(f, g, e, c)
```

### Generowanie sekwencji w oparciu o liczbę półtonów

`dźwięk + liczba` przesuwa dany dźwięk o daną liczbę półtonów. Pozwala to tworzyć następujące sekwencje:

```
> c4 + (0, 1, 2, 3)
(c4, c#4, d4, d#4)
```

Podobny efekt można uzyskać funkcję `up` jako generator wartości liczbowych:

```
> up 5
(0, 1, 2, 3, 4)
> c4 + up 4
(c4, c#4, d4, d#4, e4)
> play (c4 + up 4)
```

Analogicznie działa funkcja `down`, generująca wartości liczbowe w kierunku zera:

```
> down 5
(4, 3, 2, 1, 0)
> c4 + down 5
(e4, d#4, d4, c#4, c4)
> play (c4 + down 5)
```

Możemy złączyć sekwencje przy pomocy operatora `&`:

```
> up 5 & down 5
(0, 1, 2, 3, 4, 4, 3, 2, 1, 0)
> play (c4 + up 5 & down 5)
```

Jeśli przeszkadza nam podwójnie zagrana nuta `e4` możemy ją wyeliminować funkcją `uniq`, która usuwa sąsiadujące ze sobą powtórzenia:

```
> uniq (up 5 & down 5)
(0, 1, 2, 3, 4, 3, 2, 1, 0)
> play (c4 + uniq (up 5 & down 5))
```

## Metody odtwarzania sekwencji

Musique posiada trzy główne metody odtwarzania sekwencji:

- `play` które odtwarza dźwięki w kolejności
- `par` które odtwarza pierwszy dźwięk równocześnie z pozostałymi
- `sim` który odtwarza każdą podaną mu sekwencję równocześnie

Każda z nich w momencie napotkania dźwięku z nieokreśloną oktawą lub długością wypełni ją według wartości *domyślnej* (wartości domyślne to: 4 oktawa, 120 bpm, ćwierćnuta).

Wartości domyślne można ustawiać przy pomocy funkcji `oct`, `len` oraz `bpm`. Poniższy przykład ustawia domyślną oktawę na 5, domyślną długość na półnutę oraz domyślne tempo na 100 uderzeń na minutę:

```
> oct 5
> len hn
> bpm 100
```

### `play`

`play` przechodzi kolejno przez elementy sekwencji i kończy się po odtworzeniu wszystkich dźwięków.

Jeśli chcemy wykonać kilka `play` po sobie, możemy to osiągnąć rozdzielając je przecinkami – analogicznie do wspomnianych wcześniej sekwencji:

```
play (c qn, e qn, g hn),
play (16 * (f sn)),
play (c qn, e qn, g hn),
```

Uwaga – powyższy przykład reprezentuje polecenia zapisane w pliku (stosujemy konwencję rozszerzenia `.mq`) – wskazuje na to brak znaku zachęty `>`.

Można ten zapis uprościć:

```
play (
	(c qn, e qn, g hn),
	(16 * (f sn)),
	(c qn, e qn, g hn),
),
```

gdzie wcięcia i nowe linie zostały dodane dla poprawy czytelności.

### `par`

`par` umożliwa wykonanie pierwszego argumentu przez całą długość trwania pozostałych - niezależnie od tego jaka długość dźwięku została określona dla pierwszego z nich, zostanie dopasowany do sumy długości pozostałych dźwięków.

Przykład inspirowany Odą do Radości, w którym akord trwa przez całą długość odtwarzania sekwencyjnie pozostałych dźwięków. Wcześniej ustawiamy domyślną oktawę jako 5, by nie musieć powtarzać jej przy każdym dźwięku:

```
oct 5,
par (chord c3 g3 c4) (e, e, f, g)
```

### `sim`

`sim` umożliwia wykonywanie równocześnie kilku sekwencji.

Poniżej "niepoprawna" metoda stworzenia akordu poprzez równocześne odtworzenie dźwięków c, e, g:

```
> sim c e g
```

Trzy dźwięki C w domyślnej oktawie o długości ćwierćnuty odtwarzane równocześnie wraz z pauzą ćwierćnutową i czterema dźwiękami D w piątek oktawie o długości szesnastki:

```
> sim (3 * c qn) (p qn, 4 * (d5 sn))
```
