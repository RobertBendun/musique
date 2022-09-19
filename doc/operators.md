# Lista wbudowanych operatorów języka Musique

## Operatory arytmetyczne

- `+` – operator:
	- dodawania (liczby całkowite i ułamki):
```
> 2 + 3
5
> (1/3) + (1/4)
7/12
```
	- inkrementacji zmiennej (liczby lub akordu, w tym w szczególności pojedynczeń nuty):
```
> c + 1
c#
> c4 + 2
chord[d; f#]
```

- `-` – operator odejmowania/dekrementacji – analogicznie do operatora `+`;

- `*` – operator:
	- mnożenia (liczby całkowite i ułamki):
```
> 2 * 5
10
> (1/3) * (1/4)
1/12
```
	- powtarzania – zwraca określoną liczbę powtórzeń danego dźwięku:
> a * 4
[a; a; a; a]
> a3 * 4
[chord[a; c]; chord[a; c]; chord[a; c]; chord[a; c]]
```

- `/` – operator dzielenia:
```
> 4 / 2
2
> 5 / 7
5/7
> (2/3) / (3/5)
10/9
```
- `%` – operator działania modulo (wynikiem jest reszta z dzielenia) – *nie działa dla ułamków*:
```
> 3 % 3
0
> 6 % 5
1
```
- `**` – operator potęgowania – *wykładnikiem nie może być ułamek*:
```
> 2 ** 8
256
> (2/3) ** 3
8/27
```

## Operatory porównawcze

- `!=` – operator "nie równa się":
```
> 2 != 3
true
> a != a
false
```
- `<` – operator "mniejsze niż":
```
> 2 < 3
true
> a < a
false
```
- `<=` – operator "mniejsze lub równe":
```
> 2 <= 3
true
> a <= a
true
> 3 <= 2
false
```
- `==` – operator "równe":
```
> 2 == 3
false
> a == a
true
```
- `>` – operator "większe niż":
```
> 2 > 3
false
> a > a
true
```
- `>=` – operator "większe lub równe":
```
> 2 >= 3
false
> a >= a
true
> 3 >= 2
true
```
## Pozostałe operatory

- `.` – operator indeksu (zwraca wskazany element, liczone od 0):
```
> A:=[1; 3; 5]
> A . 1
3
```
	- indeksu
- `&` – operator łączenia:
```
> c & a
chord[c; a]
> [1;3;5] & [2;4;6]
[1; 3; 5; 2; 4; 6]
```
