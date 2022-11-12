---
title: Wprowadzenie do języka Musique
date: 2022-11-12
author: Robert Bendun <robben@st.amu.edu.pl>
---

# Wprowadzenie do języka Musique

W poniższej dokumentacji obowiązują następujące konwencje:

`>` jest to [znak zachęty](https://pl.wikipedia.org/wiki/Znak_zach%C4%99ty) i wskazuje gotowość programu Musique do wprowadzania tekstu.
Indykuje on obecność w sesji programu Musique.

Przykładowo:

```
> 3 + 4
7
>
```

prezentuje efekt wpisania tekstu `3 + 4` oraz potwierdzenia przyciskiem `ENTER`, na co Musique zareagowało wynikiem tej operacji.

Jeśli przykład terminala nie przedstawia interaktywnej sesji, a zawartość pliku to znak zachęty nie występuje na początku linii.
Nie ma różnic pomiędzy wykonywaniem komend języka Musique z pliku lub z sesji interaktywnej, różnią się tylko wygodą użytkownika.

Można wykonać polecenia rozpisane na kilka linijek w sesji interaktywnej - należy wtedy wpisać je w formie jednej linii.

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

lub wiele nut rozdzielając je przecinkiem:

```
> play (c, e, g)
```

Dostępne dźwięki bazowe to kolejno: `c`, `d`, `e`, `f`, `g`, `a`, `b`. Można je modyfikować poprzez znaki:

- `#` lub `s` podwyższający wysokość dźwięku o pół tonu (**s**harp)
- `b` lub `f` obniżający wysokość dźwięku o pół tonu (**b**mol, **f**lat)

pozwala to przykładowo tworzyć:

- `c#` - dźwięk cis
- `c##` - dźwięk d
- `eb` - dźwięk dis

Aby dźwięk został odtworzony w odpowiedniej oktawie można podać ją zaraz przy dźwięku - `c4` to dźwięk c w 4 oktawie, `c#8` to dźwięk cis w 8 oktawie.

```
> play (c4, c5, c6)
```

Aby dźwięk trwał określoną długość należy podać jako parametr pożądaną długość:

Sekwencja złożona z: ćwierćnuty c w 4 oktawie, półnuty d w 5 oktawie oraz całej nuty e w 4 oktawie wygląda następująco:

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

Korzystając z tych długości możemy uprościć powyższy przykład:

```
> play (c4 qn, d5 hn, e4 1)
```

### Pauzy

Pauzy stosuje się poprzez zastąpienie nazwy dźwięku literą `p`.

```
> play (c4 hn, p qn, c4 hn, p qn, c4 hn)
```

### Akordy

Aby odegrać akord należy umieścić sekwencję dźwięków na niego się składających w wywołaniu funkcji `chord`:

```
> play (chord (c, e, g))
```

W utworze możemy chcieć częściej używać tych samych akordów bądź sekwencji dźwięków. Możemy zachować je pod nową nazwą:

```
> sekwencja := (c, e, g)
> sekwencja
(c, e, g)
```

(wpisanie samego wyrażenia takiego jak nazwa pozwala zobaczyć jej zawartość).

Z utworzonej sekwencji możemy stworzyć akord:

```
> C := chord sekwencja
> C
chord (c, e, g)
```

i następnie go odtworzyć:

```
> play C
```

należy zwrócić uwagę, że Musique rozróżnia wielkość liter. Ponadto nazw będących oznaczeniami dźwięków jak `c` bądź `cffff` nie można używać jako nazw dla zmiennych.

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

### Losowe ustawienie elementów sekwencji

Funkcja `shuffle` pozwala na tworzenie sekwencji z losową kolejnością elementów, np:

```
> shuffle (c, e, f, g)
(c, g, e, f)
```

### Generowanie sekwencji w oparciu o liczbę półtonów

`dźwięk + liczba` przesuwa dany dźwięk o daną liczbę półtonów. Pozwala to tworzyć następujące sekwencje:

```
> c4 + (0, 1, 2, 3)
(c4, c#4, d4, d#4)
```

lub krócej wykorzystując funkcję `up`.

```
> up 5
(0, 1, 2, 3, 4)
> c4 + up 4
(c4, c#4, d4, d#4, e4)
> play (c4 + up 4)
```

lub jeśli chcemy zagrać w odwrotnej kolejności funkcję `down`:

```
> down 5
(4, 3, 2, 1, 0)
> c4 + down 5
(e4, d#4, d4, c#4, c4)
> play (c4 + down 5)
```

możemy złączyć sekwencje przy pomocy operatora `&`:

```
> up 5 & down 5
(0, 1, 2, 3, 4, 4, 3, 2, 1, 0)
> play (c4 + up 5 & down 5)
```

jeśli przeszkadza nam podwójnie zagrana nuta `e4` możemy ją wyeliminować funkcją `uniq` która usuwa sąsiadujące ze sobą duplikaty:

```
> uniq (up 5 & down 5)
(0, 1, 2, 3, 4, 3, 2, 1, 0)
> play (c4 + uniq (up 5 & down 5))
```


## Metody odtwarzania sekwencji

Musique posiada trzy główne metody otwarzania sekwencji:

- `play` które odtwarza kolejne dźwięki po kolei
- `par` które odtwarza pierwszy dźwięk równocześnie z pozostałymi
- `sim` który odtwarza każdą podaną mu sekwencję równocześnie

Każda z nich w momencie napotkania dźwięku z nieokreśloną oktawą lub długością
wypełni tą wartość według wartości *domyślnej* (wartości domyślne to: 4 oktawa, 120 bpm, ćwierćnuta).

Wartości domyślne można ustawiać przy pomocy funkcji `oct`, `len` oraz `bpm`.

### `play`

`play` przechodzi kolejno przez kolejne elementy sekwencji. `play` kończy wykonanie po odtworzeniu
wszystkich dźwięków na niego się składających.

Jeśli chcemy wykonać kilka `play` po sobie, możemy to osiągnąć rozdzielając je przecinkami - identycznie jak wcześniej tworzone sekwencje:

```
play (c qn, e qn, g hn),
play (16 * (f sn)),
play (c qn, e qn, g hn),
```

można zapisać prościej poprzez:

```
play (
	(c qn, e qn, g hn),
	(16 * (f sn)),
	(c qn, e qn, g hn),
),
```

gdzie wcięcia i entery zostały dodane dla poprawy czytelności.

### `par`

`par` umożliwa wykonanie pierwszego argumentu przez całą długość trwania pozostałych -
niezależnie od tego jak dźwięk został określony zostanie dopasowany do drugiej części.

Przykład inspirowany Odą do Radości, w którym akord trwa przez całą długość odtwarzania
sekwencyjnie pozostałych dźwięków. Wcześniej ustawiamy domyślną oktawę jako piątą,
by nie musieć powtarzać jej przy każdym dźwięku:

```
oct 5,
par (chord c3 g3 c4) (e, e, f, g)
```

### `sim`

`sim` umożliwia wykonywanie równocześnie kilku sekwencji:

Brutalna metoda stworzenia akordu poprzez równocześne odtworzenie dźwięków c, e, g:

```
> sim c e g
```

Trzy ćwierćnuty c4 oraz równocześnie po ćwierćnutowej pauzie 4 szesnastki d5:

```
> sim (3 * c qn) (p qn, 4 * (d5 sn))
```
