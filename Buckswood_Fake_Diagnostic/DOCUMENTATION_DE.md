# Buckswood Fake Diagnostic

Deutschsprachige Dokumentation fuer DaVinci Resolve

Version: 2.0.0  
Plugin-Typ: OpenFX / OFX  
Host: DaVinci Resolve  
Kategorie in Resolve: `Buckswood`  
Plugin-Name: `Buckswood Fake Diagnostic`

## Kurzbeschreibung

`Buckswood Fake Diagnostic` ist ein Diagnose- und Assistenz-Tool fuer AI-Footage, CG-Elemente und Compositing-Shots, die im Review haeufig mit dem Satz beschrieben werden:

> It feels a bit fake.

Das Plugin behandelt diesen Satz nicht als Geschmacksurteil, sondern als Diagnoseproblem. Es sucht im Bild nach typischen visuellen Signalen, die ein Element kuenstlich wirken lassen koennen:

- zu glatte, plastische Oberflaechen
- harte oder digitale Highlights
- zu scharfe Kanten oder Matte-Probleme
- auffaellige Sättigung, Gamut- oder Grade-Probleme
- fehlende Sensor-Textur und Mikrostruktur
- fehlende oder unpassende zeitliche Bewegung zwischen Frames

Das Plugin ist bewusst kein One-Click-Look. Es soll helfen, schneller zu erkennen, warum ein Bild oder Element unecht wirkt.

## Installation

Empfohlene Installation:

1. DaVinci Resolve komplett schliessen.
2. `Buckswood_Fake_Diagnostic_Installer.dmg` oeffnen.
3. `Buckswood_Fake_Diagnostic_Installer.pkg` starten.
4. Den Installer abschliessen.
5. DaVinci Resolve neu starten.

Installierter Pfad:

```text
/Library/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle
```

In Resolve findest du das Plugin hier:

```text
Color Page > OpenFX > Buckswood > Buckswood Fake Diagnostic
```

## Grundidee

Das Plugin analysiert jeden Pixel zusammen mit seinen Nachbarpixeln. In v2 kann es zusaetzlich den vorherigen und naechsten Frame lesen, um zeitliche Auffaelligkeiten zu erkennen. Daraus entstehen mehrere interne Scores. Jeder Score steht fuer eine moegliche Ursache von "fake":

- `Plastic/Smoothness`
- `Highlight/Clip`
- `Edge/Matte`
- `Grade/Gamut`
- `Texture`
- `Temporal Motion/Stability`

Aus diesen Einzelwerten wird ein Gesamtwert berechnet. Dieser Gesamtwert zeigt, wie stark ein Bildbereich nach einem typischen Fake-Signal aussieht.

## View Modes

### Diagnostic Overlay

Dieser Modus legt eine farbige Diagnose ueber das Originalbild.

Gut fuer:

- schnelle Review-Analyse
- visuelles Finden kritischer Bildbereiche
- erste Diagnose, bevor man korrigiert

### Problem Matte

Dieser Modus zeigt eine Schwarz-Weiss-Maske.

Interpretation:

- Schwarz: unkritisch
- Weiss: stark auffaellig

Gut fuer:

- Pruefen, ob das Plugin die richtigen Bildbereiche erkennt
- Erstellen einer Diagnose-Matte
- kontrolliertes Einstellen der Gewichte

### Reality Match Assist

Dieser Modus zeigt keine Diagnosefarben, sondern versucht eine subtile Korrektur.

Er kann:

- Mikro-Kontrast erhoehen
- harte Kanten leicht entschaerfen
- Highlights weicher abrollen lassen
- extreme Sättigung etwas zuegeln
- leichte Sensor-Textur hinzufuegen

Gut fuer:

- schnelle Look-Tests
- AI-Footage weniger plastisch wirken lassen
- eine Richtung fuer den finalen Grade finden

### Assist + Overlay

Dieser Modus kombiniert Korrektur und Diagnose.

Gut fuer:

- sehen, was korrigiert wurde
- technische Kommunikation mit Compositing oder Grading
- Before/After-Einschaetzung mit Diagnosehilfe

### Category Colors

Dieser Modus zeigt nur die Problemkategorie als False Color.

Farben:

- Magenta: Plastic / zu glatt
- Gelb: Highlights / Clipping
- Cyan: Kanten / Matte / Edge-Probleme
- Rot: Grade / Gamut / Sättigung
- Blau: fehlende Textur
- Gruen: Temporal / Bewegung / Flicker

Gut fuer:

- sehr schnelle Ursachenanalyse
- VFX-Supervision
- Erklaeren gegenueber Director, Compositor oder Colorist

### Temporal Stability Matte

Dieser Modus zeigt nur den zeitlichen Risiko-Score als Schwarz-Weiss-Maske.

Gut fuer:

- Bereiche finden, die zu statisch oder zu flickerig wirken
- AI-Footage mit fehlender Kamera-Mikrobewegung pruefen
- CG-Elemente auf unpassende Frame-zu-Frame-Stabilitaet checken

### Temporal Difference Overlay

Dieser Modus zeigt die Frame-zu-Frame-Differenz als farbiges Overlay.

Gut fuer:

- Motion-Jitter-Probleme schneller erkennen
- Flackern durch Grade, Highlights oder AI-Artefakte sichtbar machen
- vor dem Assist-Modus sehen, ob Temporal wirklich das Hauptproblem ist

## Parameter

### View Mode

Waehlt die Ausgabeansicht:

- `Diagnostic Overlay`
- `Problem Matte`
- `Reality Match Assist`
- `Assist + Overlay`
- `Category Colors`
- `Temporal Stability Matte`
- `Temporal Difference Overlay`

### Sensitivity

Steuert, wie empfindlich das Plugin auf Fake-Signale reagiert.

Empfehlung:

- `0.5 - 0.8`: konservativ
- `1.0`: Standard
- `1.5 - 3.0`: aggressive Diagnose

### Overlay Strength

Steuert, wie stark die Diagnosefarben ueber dem Bild liegen.

Nur relevant fuer:

- `Diagnostic Overlay`
- `Assist + Overlay`

### Correction Strength

Steuert die Staerke der automatischen Assist-Korrektur.

Nur relevant fuer:

- `Reality Match Assist`
- `Assist + Overlay`

Empfehlung:

- `0.10 - 0.25`: subtil
- `0.35`: Standard
- `0.50+`: deutlich sichtbar

### Temporal Analysis

Aktiviert den Vergleich mit Nachbarframes.

Empfehlung:

- eingeschaltet lassen fuer AI-, CG- und Compositing-Shots
- ausschalten, wenn Resolve in einem bestimmten Host-Setup keine Nachbarframes liefert

### Temporal Frame Offset

Waehlt, wie weit der Vergleich nach vorne und hinten greift.

Empfehlung:

- `1 Frame`: Standard fuer Diagnose und Assist
- `2 Frames`: etwas groesserer Bewegungsvergleich
- `3 Frames`: nur nutzen, wenn das Material langsam oder sehr stabil ist

### Plastic/Smooth Weight

Steuert, wie stark glatte, plastische Flaechen gewichtet werden.

Hoeher setzen, wenn:

- AI-Haut zu glatt wirkt
- Oberflaechen zu sauber sind
- Details fehlen

### Highlight/Clip Weight

Steuert, wie stark harte Highlights und Clipping erkannt werden.

Hoeher setzen, wenn:

- helle Stellen digital wirken
- Highlights nicht filmisch abrollen
- CG-Lichter zu sauber oder flach aussehen

### Edge/Matte Weight

Steuert, wie stark harte Kanten bewertet werden.

Hoeher setzen, wenn:

- Comps ausgeschnitten wirken
- Kanten zu scharf sind
- Matte-Probleme vermutet werden

### Grade/Gamut Weight

Steuert, wie stark Sättigung, Gamut und Grade-Risiken bewertet werden.

Hoeher setzen, wenn:

- das Element farblich nicht in die Plate passt
- eine neue LUT den VFX-Shot zerlegt hat
- Farben zu videoartig wirken

### Texture Weight

Steuert, wie stark fehlende Mikrostruktur bewertet wird.

Hoeher setzen, wenn:

- AI-Footage zu clean ist
- Sensorrauschen fehlt
- das Material nicht wie echtes Kameramaterial wirkt

### Temporal Weight

Steuert, wie stark zeitliche Auffaelligkeiten in den Gesamt-Score einfliessen.

Hoeher setzen, wenn:

- AI-Footage zu stabil wirkt
- CG-Elemente nicht mit der Plate-Mikrobewegung leben
- Highlights oder Grade ueber mehrere Frames flackern

### Micro Contrast Assist

Fuegt im Assist-Modus vorsichtig Mikro-Kontrast zurueck.

Gut fuer:

- AI-Footage
- zu weiche Renderings
- flache Oberflaechen

### Edge Soften Assist

Weicht harte Kanten im Assist-Modus leicht auf.

Gut fuer:

- Compositing-Kanten
- zu perfekte CG-Silhouetten
- AI-Kanten mit Cutout-Gefuehl

### Highlight Rolloff Assist

Komprimiert helle Bereiche etwas weicher.

Gut fuer:

- digitale Highlights
- harte Weiss-Clips
- unfilmische Spitzlichter

### Gamut Guard Assist

Zuegelt extreme Sättigung und Farbausreisser.

Gut fuer:

- LUT-Probleme
- AI-Farben
- VFX-Elemente, die nicht in die Plate passen

### Sensor Texture Assist

Fuegt eine leichte synthetische Sensor-Textur hinzu.

Gut fuer:

- sehr cleanes AI-Footage
- CGI ohne Kameracharakter
- sterile Flächen

### Temporal Smooth Assist

Mischt im Assist-Modus sehr vorsichtig Nachbarframe-Information ein.

Gut fuer:

- leichte AI-Flicker-Artefakte
- kleine Highlight-Spruenge
- subtile Temporal-Stabilisierung

Wichtig: Dieser Regler sollte sehr niedrig bleiben. Starte bei `0.10 - 0.25`.

### Texture Seed

Veraendert das Muster der Sensor-Textur.

## Empfohlener Workflow

### 1. Diagnose starten

Setze `View Mode` auf:

```text
Diagnostic Overlay
```

Pruefe, welche Bereiche farbig markiert werden.

### 2. Ursache isolieren

Wechsle auf:

```text
Category Colors
```

Damit erkennst du, ob das Problem eher von Kanten, Highlights, Textur, Grade oder Plasticness kommt.

### 3. Temporal pruefen

Wenn der Shot bewegt ist, pruefe zusaetzlich:

```text
Temporal Stability Matte
```

oder:

```text
Temporal Difference Overlay
```

So erkennst du, ob der Fake-Eindruck eher aus fehlender Mikrobewegung, Flackern oder unpassender Frame-Stabilitaet kommt.

### 4. Maske pruefen

Wechsle auf:

```text
Problem Matte
```

Wenn die Matte zu viel erkennt:

- `Sensitivity` senken
- einzelne Gewichte reduzieren

Wenn die Matte zu wenig erkennt:

- `Sensitivity` erhoehen
- relevante Gewichte erhoehen

### 5. Vorsichtige Korrektur testen

Wechsle auf:

```text
Reality Match Assist
```

Starte mit:

```text
Correction Strength: 0.10 - 0.25
```

Erhoehe nur, wenn die Korrektur wirklich hilft.

Nutze `Temporal Smooth Assist` nur leicht. Meist reichen `0.10 - 0.25`.

### 6. Final vergleichen

Nutze Bypass oder Node On/Off in Resolve, um das Ergebnis zu vergleichen.

Wenn das Bild besser wirkt, aber noch nicht final ist:

- Plugin als Diagnose behalten
- finalen Look im Grading oder Compositing nachbauen

## Typische Anwendungsfaelle

### AI-Footage wirkt plastisch

Empfohlene Parameter:

- `Plastic/Smooth Weight`: hoch
- `Texture Weight`: hoch
- `Micro Contrast Assist`: leicht erhoehen
- `Sensor Texture Assist`: leicht erhoehen

### CG-Element wirkt ausgeschnitten

Empfohlene Parameter:

- `Edge/Matte Weight`: hoch
- `Edge Soften Assist`: vorsichtig erhoehen
- `Diagnostic Overlay` oder `Category Colors` nutzen

### Highlights wirken digital

Empfohlene Parameter:

- `Highlight/Clip Weight`: hoch
- `Highlight Rolloff Assist`: erhoehen
- `Gamut Guard Assist`: leicht erhoehen

### Element passt farblich nicht zur Plate

Empfohlene Parameter:

- `Grade/Gamut Weight`: hoch
- `Gamut Guard Assist`: erhoehen
- danach im Resolve-Grade final matchen

## Was das Plugin nicht macht

Diese Version ist kein vollautomatisches VFX-QC-System. v2 kann Nachbarframes lesen und eine Temporal-Diagnose ausgeben, bleibt aber bewusst konservativ.

Sie kann aktuell nicht:

- eine Matte gegen eine separate Plate analysieren
- Optical Flow oder Tracking-Probleme eindeutig erkennen
- eine vollautomatische VFX-QC ersetzen
- echtes Machine Learning ausfuehren

Sinnvolle naechste Ausbaustufen:

- Multi-Input Plate vs Element Match
- Matte Edge Compare
- Shot-Level Report
- Premiere-native `.prm` Version

## Praktische Review-Formulierungen

Statt:

```text
It feels fake.
```

Kann man mit dem Plugin praeziser sagen:

```text
The highlights are clipping differently from the plate.
```

```text
The edge response is too clean compared to the camera image.
```

```text
The AI element lacks sensor texture in the midtones.
```

```text
The grade is pushing the VFX element out of the plate gamut.
```

Das ist der eigentliche Nutzen: Es uebersetzt ein vages Review-Gefuehl in eine technische Richtung.

## Troubleshooting

### Plugin erscheint nicht in Resolve

1. Resolve komplett beenden.
2. Plugin-Pfad pruefen:

```text
/Library/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle
```

3. Resolve neu starten.
4. In OpenFX nach `Buckswood` suchen.

### Plugin erscheint, aber Bild bleibt unveraendert

Pruefe:

- `View Mode`
- `Sensitivity`
- `Overlay Strength`
- `Correction Strength`

Wenn `Reality Match Assist` aktiv ist und `Correction Strength` auf `0` steht, wird kaum etwas veraendert.

### Overlay ist zu stark

Reduziere:

```text
Overlay Strength
Sensitivity
```

### Korrektur ist zu sichtbar

Reduziere:

```text
Correction Strength
Sensor Texture Assist
Micro Contrast Assist
Edge Soften Assist
```

## Distribution

Aktuelle Installer-Artefakte:

```text
Buckswood_Fake_Diagnostic_Installer.dmg
Buckswood_Fake_Diagnostic_Installer.pkg
```

Beide Dateien sind mit Developer ID signiert und notarisiert.

## Kurzfazit

`Buckswood Fake Diagnostic` ist ein Supervisor-orientiertes Diagnose-Tool. Es macht unsichtbare Fake-Signale sichtbar und bietet eine vorsichtige Assist-Korrektur.

Die Staerke des Plugins liegt nicht darin, ein finales Bild automatisch zu erzeugen, sondern darin, schneller die richtige Ursache zu finden.
