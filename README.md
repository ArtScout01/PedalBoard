# PedalBoard (VST3)

Zwei Pedalketten (A und R) auf je einem Pedalboard mit Teppich-Hintergrund. Pedale lassen sich am Titelbalken ziehen (Reihenfolge aendern), per Fussschalter ein-/ausschalten.

## Bauen ueber GitHub
1. Alle Dateien (auch den Ordner `.github`) in ein GitHub-Repo hochladen.
2. Reiter **Actions** -> Workflow `build` -> nach dem gruenen Haken unter **Artifacts** `PedalBoard-Windows` / `PedalBoard-macOS` laden.
3. `VST3/PedalBoard.vst3` in den VST3-Ordner kopieren (Windows: `C:\Program Files\Common Files\VST3`, macOS: `~/Library/Audio/Plug-Ins/VST3`, dort einmalig `xattr -dr com.apple.quarantine <.vst3>`).
