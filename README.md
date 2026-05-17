# ESP32 VFO z generatorem Si5351A

Sterownik VFO zbudowany na ESP32-WROOM-32 i syntezerze Si5351A. Projekt generuje przestrajany sygnal w zakresie od 100 kHz do 225 MHz, pokazuje czestotliwosc na kolorowym wyswietlaczu TFT oraz udostepnia wygodna obsluge przez enkoder i piec przyciskow.

Repozytorium zawiera firmware ESP-IDF/PlatformIO oraz projekt PCB i schemat w KiCad.

## Najwazniejsze funkcje

- Zakres VFO: `100 kHz .. 225 MHz`.
- Syntezer Si5351A sterowany przez I2C 400 kHz.
- Wyjscia Si5351:
  - `CLK1` - glowny sygnal LO/VFO,
  - `CLK0` - nosna I,
  - `CLK2` - nosna Q, przesunieta wzgledem I.
- Kolorowy wyswietlacz ST7735 1.8", framebuffer `160 x 128 px`.
- Analogowa tarcza VFO oraz cyfrowy odczyt czestotliwosci.
- Enkoder kwadraturowy PCNT z dynamicznym przyspieszaniem strojenia.
- Kroki strojenia: `10 Hz`, `100 Hz`, `1 kHz`, `10 kHz`, `100 kHz`, `1 MHz`.
- Dziesiec komorek pamieci `M0..M9`.
- Autozapis ostatniej czestotliwosci do NVS po okresie bezczynnosci.
- Menu pasm z gotowymi czestotliwosciami startowymi.
- Blokada strojenia `LOCK`.
- Kalibracja czestotliwosci kwarcu Si5351 w zakresie `-5000..+5000 Hz`.
- Regulacja jasnosci podswietlenia PWM.
- Oddzielne srodowiska PlatformIO: `esp32_release` i `esp32_debug`.

## Sprzet

Glownymi elementami ukladu sa:

- ESP32-WROOM-32,
- modul Si5351A,
- wyswietlacz TFT 1.8" ze sterownikiem ST7735,
- enkoder obrotowy z przyciskiem,
- 5 przyciskow tact switch,
- podswietlenie wyswietlacza sterowane PWM,
- plytka PCB zaprojektowana w KiCad.

Pliki KiCad sa w katalogu `Kicad files/`.

## Polaczenia GPIO

| Funkcja | GPIO |
| --- | ---: |
| Enkoder A | 17 |
| Enkoder B | 16 |
| Przycisk enkodera SW | 34 |
| STEP_DN | 26 |
| STEP_UP | 27 |
| MEM | 25 |
| SAVE | 32 |
| BAND | 33 |
| Si5351 SDA | 21 |
| Si5351 SCL | 22 |
| ST7735 SCLK | 18 |
| ST7735 MOSI | 23 |
| ST7735 CS | 5 |
| ST7735 DC | 2 |
| ST7735 RST | 15 |
| Podswietlenie TFT | 4 |

Przyciski sa aktywne stanem niskim. W tej konstrukcji zewnetrzne rezystory pull-up sa wymagane dla GPIO16, GPIO17, GPIO33 i GPIO34. Na plytce zastosowano 4.7 kOhm, ale praktyczny zakres to ok. 2.2 kOhm .. 10 kOhm.

## Ekran

Normalny widok VFO pokazuje:

- analogowa tarcze strojenia,
- cyfrowa czestotliwosc generowana,
- aktywny krok strojenia,
- aktywny bank pamieci `VFO` albo `M1..M9`,
- graficzne overlaye dla operacji `SAVE`, `LOAD`, `LOCK`, menu pasm, jasnosci i kalibracji.

W trybie `LOCK` czestotliwosc pozostaje widoczna, a na ekranie pojawia sie duzy symbol klodki oraz napis `LOCK` na dole.

## Obsluga

### Enkoder

| Akcja | Efekt |
| --- | --- |
| Obrot w trybie VFO | Zmiana czestotliwosci wedlug aktywnego kroku |
| Szybki obrot | Automatyczne przyspieszenie strojenia `x10`, `x100`, `x1000` |
| Krotkie nacisniecie SW | Otwarcie menu pasm lub zatwierdzenie wyboru pasma |
| Dlugie nacisniecie SW | Zmiana kierunku pracy enkodera |
| Obrot w menu BAND | Przewijanie listy pasm |
| Obrot w XTAL CAL | Zmiana korekty kwarcu |
| Obrot w BRIGHTNESS | Zmiana jasnosci podswietlenia |

### Przyciski

| Przycisk | Krotkie nacisniecie | Dlugie nacisniecie |
| --- | --- | --- |
| STEP_DN | Mniejszy krok strojenia | - |
| STEP_UP | Wiekszy krok strojenia | - |
| MEM | Nastepny bank pamieci / YES w dialogu / SAVE w ekranach ustawien | Wczytanie czestotliwosci z aktywnego banku |
| SAVE | NO/CANCEL w dialogach | Otwarcie dialogu zapisu do aktywnego banku |
| BAND | Ekran regulacji jasnosci | Kalibracja kwarcu XTAL |
| STEP_DN + STEP_UP | `LOCK` po przytrzymaniu ok. 1 s | `LOCK` po przytrzymaniu ok. 1 s |

W trybie `LOCK` zmiana czestotliwosci i kroku jest blokowana. Odblokowanie odbywa sie ta sama kombinacja `STEP_DN + STEP_UP`.

## Pamiec i pasma

Firmware przechowuje parametry w NVS:

- ostatnia czestotliwosc,
- 10 komorek pamieci `M0..M9`,
- ostatni krok strojenia,
- korekta kwarcu Si5351,
- jasnosc wyswietlacza,
- ostatnio uzywany bank.

Domyslne komorki pamieci:

| Bank | Czestotliwosc |
| --- | ---: |
| M0 | 14.230 MHz |
| M1 | 3.730 MHz |
| M2 | 3.8531 MHz |
| M3 | 5.450 MHz |
| M4 | 5.505 MHz |
| M5 | 6.070 MHz |
| M6 | 7.130 MHz |
| M7 | 7.8781 MHz |
| M8 | 8.957 MHz |
| M9 | 10.100 MHz |

Menu pasm zawiera:

`30m`, `20m`, `17m`, `15m`, `12m`, `10m`, `6m` oraz `FM 100 MHz`.

## Tryby ekranowe

- `VFO` - normalna praca i strojenie.
- `SAVE TO Mx?` - potwierdzenie zapisu aktualnej czestotliwosci do pamieci.
- `SAVED` - komunikat po zapisie.
- `LOAD` - komunikat po wczytaniu pamieci.
- `SELECT BAND` - wybor pasma lub preset FM.
- `BRIGHTNESS` - regulacja jasnosci podswietlenia.
- `XTAL CALIBRATION` - korekta czestotliwosci referencyjnej Si5351.
- `LOCK` - blokada przypadkowej zmiany czestotliwosci.

## Budowanie

Projekt jest przygotowany pod PlatformIO z frameworkiem ESP-IDF.

Domyslnym srodowiskiem jest release:

```powershell
pio run
```

Jawny build release:

```powershell
pio run -e esp32_release
```

Build debug:

```powershell
pio run -e esp32_debug
```

Wgrywanie:

```powershell
pio run -e esp32_release -t upload
```

Monitor portu szeregowego:

```powershell
pio device monitor
```

Domyslny port uploadu w `platformio.ini` to `COM42`.

## Konfiguracja buildow

`platformio.ini` definiuje dwa warianty:

- `esp32_release` - normalny build firmware,
- `esp32_debug` - build debug z wyzszym poziomem logow.

Wspolne ustawienia ESP-IDF sa w `sdkconfig.defaults`. Dodatkowe ustawienia debug sa w `sdkconfig.debug.defaults`. Pelne pliki `sdkconfig.esp32_*` sa generowane przez PlatformIO lokalnie i sa ignorowane przez git.

## Struktura projektu

| Sciezka | Opis |
| --- | --- |
| `src/main.c` | Inicjalizacja systemu, zadanie wyswietlacza |
| `src/si5351.c` | Sterownik i obliczenia rejestrow Si5351 |
| `src/display.c` | Sterownik ST7735 i transfer DMA |
| `src/graph.c` | Framebuffer RGB i prymitywy graficzne |
| `src/dial.c` | Analogowa tarcza VFO |
| `src/encoder.c` | Obsluga enkodera PCNT |
| `src/buttons.c` | Obsluga przyciskow i trybow |
| `src/nvs_storage.c` | Pamiec NVS i autozapis |
| `src/ui_overlay.c` | Overlaye graficzne i menu |
| `include/config.h` | Glowne stale konfiguracyjne |
| `Kicad files/` | Projekt PCB i schemat |

## Uwagi

Projekt jest przeznaczony do pracy jako eksperymentalny generator/VFO. Przy podlaczaniu do torow radiowych nalezy zadbac o odpowiednie poziomy sygnalu, filtracje harmonicznych, ekranowanie i zgodnosc z lokalnymi przepisami dotyczacymi emisji radiowych.
