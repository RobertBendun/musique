# Lista wbudowanych operatorów języka Musique

## Operatory arytmetyczne

- `+` – operator:
	- dodawania (liczby całkowite i ułamki):

	```
	> 2 + 3
	5
	> (1/3) + (1/4)
	7/12
	> 1 + (up 10)
	[1; 2; 3; 4; 5; 6; 7; 8; 9; 10]
	```

	- inkrementacji zmiennej (liczby lub akordu, w tym w szczególności pojedynczeń nuty):

	```
	> c + 1
	c#
	 c4 + 2
	hord[d; f#]
	```

- `-` – operator odejmowania/dekrementacji – analogicznie do operatora `+`;

- `*` – operator:

	- mnożenia (liczby całkowite i ułamki):

	```
	> 2 * 5
	10
	> (1/3) * (1/4)
	1/12
	> 2 * (up 10)
	[0; 2; 4; 6; 8; 10; 12; 14; 16; 18]
	```

	- powtarzania – zwraca określoną liczbę powtórzeń danego dźwięku:

	```
	> a * 4
	[a; a; a; a]
	> a3 * 4
	[chord[a; c]; chord[a; c]; chord[a; c]; chord[a; c]]
	```

- `/` – operator dzielenia – *wektor może być wyłącznie dzielną*:

```
> 4 / 2
2
> 5 / 7
5/7
> (2/3) / (3/5)
10/9
> (up 10) / 2
[0; 1/2; 1; 3/2; 2; 5/2; 3; 7/2; 4; 9/2]
```

- `%` – operator działania modulo (wynikiem jest reszta z dzielenia) – *nie działa dla ułamków*, *wektor może być wyłącznie dzielną*:

```
> 3 % 3
0
> 6 % 5
1
> (up 10) % 2
[0; 1; 0; 1; 0; 1; 0; 1; 0; 1]
```

- `**` – operator potęgowania – *wykładnikiem nie może być ułamek*:

```
> 2 ** 8
256
> (2/3) ** 3
8/27
> (up 10) ** 2
[0; 1; 4; 9; 16; 25; 36; 49; 64; 81]
> 2 ** (up 10)
[1; 2; 4; 8; 16; 32; 64; 128; 256; 512]
```

## Operatory porównawcze

- `!=` – operator "nie równa się":

```
> 2 != 3
true
> a != a
false
> 3 != (up 5)
[true; true; true; false; true]
```

- `<` – operator "mniejsze niż":

```
> 2 < 3
true
> a < a
false
> 3 < (up 5)
[true; true; true; false; false]
```

- `<=` – operator "mniejsze lub równe":

```
> 2 <= 3
true
> a <= a
true
> 3 <= 2
false
> 3 <= (up 5)
[true; true; true; true; false]
```

- `==` – operator "równe":

```
> 2 == 3
false
> a == a
true
> 3 == (up 5)
[false; false; false; true; false]
```

- `>` – operator "większe niż":

```
> 2 > 3
false
> a > a
true
> 3 > (up 5)
[false; false; false; false; true]
```

- `>=` – operator "większe lub równe":

```
> 2 >= 3
false
> a >= a
true
> 3 >= 2
true
> 3 >= (up 5)
[false; false; false; true; true]
```

## Pozostałe operatory

- `.` – operator indeksu (zwraca wskazany element, liczone od 0):

```
> A:=[1; 3; 5]
> A . 1
3
> (up 7).4
3
```

- `&` – operator łączenia:

```
> c & a
chord[c; a]
> [1;3;5] & [2;4;6]
[1; 3; 5; 2; 4; 6]
> (up 7) & (down 9)
[0; 1; 2; 3; 4; 5; 6; 8; 7; 6; 5; 4; 3; 2; 1; 0]
```
