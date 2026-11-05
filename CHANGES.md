# Änderungen (diese Runde)

Basis: deine zuletzt hochgeladenen Dateien (PedalsCircuit.h mit den neuen Pedal-Daten, Tape.h mit neuem Flutter und Modulationsrauschen).

## Neu
- **Tape, TEAC:** Band-Typ (Standard 150 / Low Noise 203), Bias-Schalter und EQ-Schalter getrennt (Parameter `tape_type`, `tape_biassw`, `tape_eqsw`). Passende Kombination = unverändert, Fehlanpassung = Höhenverlust bzw. Höhenanhebung wie in der Handbuch-Tabelle (Größenordnung EST). Bei der Revox ohne Wirkung (Menüs ausgegraut).
- **Tape-Latenz:** `latency()` meldete 2 Samples zu viel (per Messlauf gefunden). Jetzt 102 Samples bei 48 kHz, Bypass-Pfad angepasst.
- **Tape-Frequenzgang:** Tiefpass wird nicht mehr bei 0,45 fs abgeriegelt (`Svf::set` hat einen optionalen Grenzwert). Bei 48 kHz jetzt −1,6 dB statt −6,5 dB bei 22 kHz.
- **Tape-Verzögerung:** kubische (Hermite) statt linearer Interpolation. Bei 44,1 kHz fiel der Pegel bei 15 kHz sonst um 2,2 dB, jetzt flach.
- **Editor:** Tape-Leiste neu angeordnet (der Noise-Schalter hatte vorher fast keine Höhe), Hinweiszeile zu geschätzten Werten.
- **PedalsCircuit.h:** Kommentar, der `tone.set (3000.f, fs);` im 6G15 verschluckt hatte, korrigiert.

## Tape: Hysterese (Jiles-Atherton) statt statischem tanh
- Neue Klasse `Hysteresis` in `Tape.h` (RK4 mit Unterschritten, 2x-Oversampling wie bisher). Parameter c = 0,90, k = 1,0, Drive 2,32, Gain 1,086 so kalibriert, dass die Handbuch-Werte erhalten bleiben: Klirr 0,22 / 1,07 / 3,68 / 6,35 % bei -10 / 0 / +6 / +9 VU (tanh: 0,19 / 1,07 / 3,58 / 6,37 %), Pegel bei 0 VU unverändert, Revox 0,53 % / 1,75 %.
- Wirkung: Gedächtnis (pegelabhängige Phase der Grundwelle, -0,97 Grad bei 0 VU gegenüber -30 VU). Mehr Bias verschmälert die Schleife (EST).
- Neuer Schalter **Hysterese** im Tape-Panel (Parameter `tape_hys`) zum A/B-Vergleich mit dem alten tanh.
- Frequenzabhängige Sättigung gab es schon: Pre-Emphasis vor der Nichtlinearität lässt hohe Frequenzen früher komprimieren (10 kHz: -1,4 dB bei 0 VU, -4,2 dB bei +6 VU, 400 Hz: -0,3 / -0,8 dB). Die Hysterese ändert das nur leicht.
- Rechenaufwand: etwa 9 % eines Kerns bei 48 kHz Stereo (vorher 3 %).

## Pedale: Oversampling und schnelleres Fuzz Face
- Neu in `PedalsCircuit.h`: `Oversampler<2|4>` (Zwei-Pfad-Allpass-Halbband, IIR, kein FIR-Latenzblock). Spiegelunterdrückung 73 bis 82 dB bis 18 kHz, Hin- und Rückweg flach (Tests/dsp_test.cpp, Abschnitt 9).
- **Tube Screamer:** Clipping-Stufe mit 4x Oversampling, Newton mit Abbruch bei kleinem Schritt. Aliasing bei 5,3 kHz, Drive 1,0, -20 dBFS: **-7,8 dB -> -54 dB** (48 kHz). Ungerade Harmonische gegenüber vorher innerhalb 0,4 dB.
- **Fuzz Face:** 4x statt 2x (mit richtigen Filtern statt linearer Interpolation und Mittelwert), analytische Jacobi-Matrix statt numerischer. Rechenzeit im ungünstigsten Fall **27 % statt 40 %** eines Kerns. Harmonische gegenüber vorher innerhalb 0,16 dB. Aliasing bei 5,3 kHz, Fuzz 0,3: -57 dB.
- Bekannte Grenze: Bei Fuzz 1,0 und hohen Tönen mit viel Pegel (extrem hartes Clipping) bleibt ein Rest (bei 5,3 kHz, -10 dBFS etwa -4 dB, bei -20 dBFS -16 dB). Das lässt sich nur mit noch mehr Oversampling verbessern.
- Der Oversampler verzögert TS808 und Fuzz Face um etwa 4 Samples (0,09 ms). Das wird nicht an den Host gemeldet, weil Pedale ein- und ausgeschaltet werden können.
- `Tests/alias_test.cpp` misst Aliasing und Rechenzeit aller Pedale, `Tests/pedal_ref.cpp` vergleicht Ausgaben gegen eine gespeicherte Referenz.

## Rechenzeit (ein Kern, 48 kHz, einzeln gemessen, Regler Mitte)
Fuzz Face 17,5 %, Tape (Stereo) 15,8 %, Tube Screamer 8,2 %, alle anderen Pedale zusammen etwa 12 %. Mit allen 17 Pedalen und Tape gleichzeitig und allen Reglern am Anschlag sind es über 100 % (unrealistischer Extremfall, weil sich die Verzerrer gegenseitig übersteuern). Im Plugin laufen nur eingeschaltete Pedale.

## Messlauf (ohne JUCE)
`Tests/dsp_test.cpp` prüft alle Pedale (keine NaN/Inf, Pegel) und das Tape (Klirr 1 %/3 %, Frequenzgang, Rauschen, Flutter, Latenz, Schalter).
Bauen und starten im Projektordner:

    g++ -std=c++17 -O2 -I Source -I Source/pb Tests/dsp_test.cpp -o dsp_test && ./dsp_test

## Nicht enthalten
CMakeLists.txt unverändert (deine GitHub-Version mit Apple-Einstellungen und Assets bleibt). Das Plugin selbst (JUCE-Teil) wurde nicht gebaut.
