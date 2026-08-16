# MonkeyCut – Benutzerhandbuch / User Guide

MonkeyCut schneidet Werbespots aus TV-Aufnahmen: Videodatei öffnen, Sende-
schneiden manuell markieren oder per [cutlist.at](https://www.cutlist.at)
automatisch übernehmen, fertig geschnittenes Video **ohne Neuekodierung**
exportieren. Die App läuft portabel – kein Installer, keine Abhängigkeiten.

MonkeyCut cuts ads out of TV recordings: open a video, mark the segments to
keep manually or apply a [cutlist.at](https://www.cutlist.at) cutlist
automatically, and export the result **without re-encoding**. The app is
portable – no installer, no dependencies.

## Aufbau / Layout

- **Bildschirm:** Vorschau des aktuellen Frames.
- **Zeitleiste (unten):** blaue Leiste = wird behalten; graue Lücken = werden
  entfernt. Klicken/Vorziehen springt in der Zeit.
- **Schnitttabelle:** alle Behalt-Spannen mit In/Out, Länge, Zeit bis zum
  nächsten Bildpunkt (Δ) – Zeilen werden beim Markieren live gefiltert.
- **Menü:** Datei (öffnen/speichern/exportieren/Einstellungen), cutlist.at.
  / *Screen (preview), timeline (blue = kept, gaps = removed), cut table
  (live-filtered), File and cutlist.at menus.*

## Schneller Start / Quick start

1. Videodatei per Drag & Drop auf das Fenster ziehen (oder: *Datei → Video
   öffnen…*).
2. Zum Start der ersten Sendeminute springen (Zahlenfeld oder Zeitleiste),
   `I` drücken → zum Start der Werbeunterbrechung, `O` drücken.
3. Wiederholen, bis alle Sendeteile markiert sind.
4. *Datei → Video exportieren…* – Ziel wählen, fertig.

1. Drag a video onto the window (or *File → Open video…*).
2. Jump to the start of the first kept segment, press `I`; jump to where the
   ad starts, press `O`.
3. Repeat for every kept segment.
4. *File → Export video…* – pick the target file, done.

### Tastatur / Keyboard

| Taste | Aktion / Action |
|---|---|
| `I` | Behalt-Anfang markieren / mark keep-start |
| `O` | Behalt-Ende markieren / mark keep-end |
| `←` / `→` | Ein Bild zurück/vor (funktioniert auch mit gehaltenen Pausen) / step one frame back/forward |
| `Taste` | Abspielen / Pause / play / pause |
| `Entf` | Ausgewählte Spanne entfernen / remove selected range |
| `Strg+O` | Video öffnen / open video |
| `Strg+E` | Video exportieren / export video |
| `Strg+S` | Schnittliste speichern / save cutlist |

Tastatur-Zeile oben links: Zahl eingeben = Sekunden, `MS` = Millisekunden,
`F` = Bild (z. B. `120F`). / *Top-left time field: numbers = seconds, `MS` =
milliseconds, `F` = frame (e.g. `120F`).*

### Schnapp-Funktion / Cut snapping

Schnitte an **Bildepunkten** (keyframes) auszurichten kostet kein
Neuekodieren. Beim Markieren wird der I-Punkt automatisch auf den nächsten
Bildepunkt **vor** der Markierung gezogen; Δ in der Tabelle zeigt, wie weit
das ist. Der O-Punkt bleibt exakt an der eigenen Markierung. /
*Cut starts snap to the previous keyframe (no re-encode needed); Δ in the
table shows the offset. End points stay exactly where you marked them.*

Für werbefreie Präzision: beim `O`-Markieren einen halben Satz (1–2 Frames)
**vor** der ersten Werbe-Szene stehen lassen, damit die Wiedergabe nicht
über den nächsten Bildepunkt hinausspringt. / *When marking `O`, stop 1–2
frames before the first ad scene so playback doesn't jump past the next
keyframe.*

## cutlist.at-Ablauf / cutlist.at workflow

1. Aufnahme öffnen (oder das Aufnahmeverzeichnis auswählen – MonkeyCut
   erinnert sich dafür).
2. *cutlist.at → Schnittlisten suchen…* → Serien-/Episodentyping →
   Doppelklick auf das Ergebnis lädt die CUL-Datei.
3. MonkeyCut passt die Schnittliste an:
   - **Video geöffnet + Name passt** (Tokens des Aufnahmefielens, Sender,
     Ausstrahlungsdatum, Videodateityp) → Schnittliste wird direkt
     übernommen; bei Unsicherheit gibt's eine Bestätigung.
   - **Kein Video geöffnet** → im Aufnahmeverzeichnis wird die passende
     Aufnahme gesucht (Schwelle: Token-Übereinstimmung ≥ 60 % plus Sender
     und – wenn in der CUL-Daten vorhanden – das Ausstrahlungsdatum muss
     stimmen) → „Öffnen und anwenden?".
4. Alle Behalt-Spannen erscheinen in der Tabelle – Kontrolle, dann
   *Datei → Video exportieren…*.

1. Open the recording (or pick your recordings folder – MonkeyCut remembers
   it).
2. *cutlist.at → Search for cutlists…* → type the show/episode →
   double-click a result to download its CUL.
3. MonkeyCut matches:
   - **video open + name matches** (recording filename tokens, channel,
     air date, video file type) → the cutlist is applied directly; you get
     a confirmation when unsure.
   - **no video open** → the recordings folder is scanned for the matching
     recording (token overlap ≥ 60 %, plus channel, and the air date must
     match when the CUL carries one) → „Open and apply?".
4. All keep-ranges appear in the table – review, then *File → Export
   video…*.

Hinweis: cutlist.at läuft nur über HTTP (Eigenschaft der Seite) – die
Schnittlisten sind klein, aber die Verbindung ist nicht verschlüsselt. /
*Note: cutlist.at is HTTP-only (a property of the site); the cutlists are
small, but the connection is unencrypted.*

## Schnitte aus CUL/Projekt übernehmen / Import

- *Datei → Schnittliste speichern…* schreibt die aktuelle Tabelle als
  `.cul` (kompatibel mit cutlist.at/DVBDream-Format, `FramesPerSecond=25.00`
  wird an das offene Video angepasst).
- `.cul` (auch per Drag & Drop) und `.mproject` (MonkeyCut-Projekt mit
  Video-Pfad + Spannen) können über *Datei → Projekt öffnen…* bzw. Drag &
  Drop geladen werden.

- *File → Save cutlist…* writes the current table as `.cul` (compatible
  with the cutlist.at/DVBDream format; `FramesPerSecond=25.00` adapts to the
  open video).
- `.cul` (also via drag & drop) and `.mproject` (MonkeyCut project with
  video path + ranges) load via *File → Open project…* or drag & drop.

## Export / Export

- **Kein Neuekodieren:** Video- und Audiospuren werden bit-genau kopiert
  (Streams werden umgestellt). Der I-Punkt steht daher auf einem
  Bildepunkt; der O-Punkt ist frame-genau (Audio kann um bis zu einen
  Audiorahmen (20 Ms bei MP2) abweichen).
- **Container = Quelldatei:** Aus `.ts` wird `.ts`, aus `.mp4` wird `.mp4`.
- Fortschritt läuft in der Statusleiste; das Video bleibt während des
  Exports bedienbar.
- Umkodieren (z. B. nach `.mp4`/H.264) ist **geplant, aber in v1 nicht
  verfügbar**.

- **No re-encoding:** video and audio streams are copied bit-exact.
  Starts therefore sit on a keyframe; ends are frame-exact (audio may
  drift by up to one audio frame, 20 ms for MP2).
- **Container matches the input:** `.ts` in → `.ts` out, `.mp4` in →
  `.mp4` out.
- Progress in the status bar; the app stays usable while exporting.
- Re-encoding (e.g. to `.mp4`/H.264) is **planned but not available in v1**.

## Einstellungen / Settings

*Datei → Einstellungen…*:

- **Sprache:** Deutsch (Standard) / English – wird nach Neustart wirksam.
- **cutlist.at-Server:** Standard `http://www.cutlist.at`.

- **Language:** German (default) / English – applied after restart.
- **cutlist.at server:** default `http://www.cutlist.at`.

Einstellungen landen in der Konfiguration des Betriebs-/Linux-Users
(Linux: `~/.config/MonkeyCut/...`). / *Settings go to the user's Qt
configuration store (Linux: `~/.config/...`).*

## Hinweise & Fehler / Tips & troubleshooting

- **VFR-Warnung** (variable Frame-Rate): Schnitte sind dann nur
  sekunden-genau, nicht bild-genau – bei TV-Aufnahmen selten ein Problem.
- **Mehrere Video-/Audiospuren:** MonkeyCut nimmt die erste jedes Typs und
  zeigt eine Warnung.
- **Nicht sprungfähig** (z. B. reine Datenströme): Scrubbing wird langsam,
  Funktion bleibt erhalten.
- **Export „nichts zu exportieren"**: Die Tabelle ist leer (z. B. weil das
  Video zu kurz ist oder alle Spanten entfernt wurden).
- **CUL passt nicht**: Der Abgleich ist konservativ bewusst – bei
  Uneinigkeit Video und CUL manuell zusammenführen (Video öffnen →
  Schnittliste-Dialog erneut, oder `.cul` importieren).

- **VFR warning** (variable frame rate): cuts are only second-accurate –
  rarely a problem for TV recordings.
- **Multiple video/audio streams:** the first of each type is used, with a
  warning.
- **Not seekable** (raw data streams): scrubbing is slow but functional.
- **Export "nothing to export"**: table is empty (video too short or all
  ranges removed).
- **Cutlist doesn't match**: matching is deliberately conservative – merge
  manually (open the video, re-open the cutlist dialog, or import the
  `.cul`).