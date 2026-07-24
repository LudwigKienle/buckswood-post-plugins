# Buckswood Cinematic Tools v2.1

## Installation

1. DaVinci Resolve vollständig beenden.
2. `Buckswood_Cinematic_Tools_v2_Installer.pkg` ausführen.
3. Resolve neu starten.
4. `Color > Effects > Buckswood` öffnen.

Die Suite enthält drei getrennte Effekte.

## Frame Director

Frame Director hilft bei der Wahl und Vorschau eines filmischen Bildausschnitts.

Empfohlener Ablauf:

1. `View` auf `Composition Guides` stellen.
2. Zielseitenverhältnis wählen.
3. `Framing Mode` auf `Auto Composition` lassen.
4. Mit `Horizontal Offset` und `Vertical Offset` korrigieren.
5. `View` auf `Cinematic Crop` zurückstellen.

### Wichtige Regler

- `Target Aspect Ratio`: 2.39:1, 2.00:1, 1.85:1, 16:9, 4:3, 1:1 oder 9:16.
- `Subject Detail Weight`: priorisiert kontrastreiche und strukturierte Motive.
- `Skin Priority`: erhöht die Gewichtung warmer, hautähnlicher Bereiche.
- `Subject Lock`: hält einen festgelegten Bildpunkt unabhängig von wechselnder Saliency.
- `Locked Subject X/Y`: Position des zu schützenden Motivs im Quellbild.
- `Shot Cut Sensitivity`: verhindert, dass die Glättung Informationen über einen Schnitt hinweg mischt.
- `Camera Path Smoothing`: reduziert springende Crop-Positionen.
- `Maximum Reframe Step`: begrenzt die virtuelle Kamerabewegung.
- `Subject Height`: legt fest, wie hoch das erkannte Motiv im Crop sitzt.

`Cinematic Crop` rendert keine Hilfslinien. `Composition Guides` ist nur für die
Einrichtung gedacht.

## Radiance Recover

Radiance Recover erweitert vorhandene SDR-Tonwerte in einem Float-Workflow und
rekonstruiert lokale Highlight- und Schattenstruktur konservativ.

Empfohlener Ablauf:

1. Effekt vor dem kreativen Look anwenden.
2. `Recovery Confidence Matte` prüfen.
3. `Highlight Reconstruction` langsam erhöhen.
4. `Recovered Headroom Stops` normalerweise zwischen 1 und 2.5 halten.
5. Mit `Output Mix` zum Original zurückmischen.

### Ansichten

- `Recovered Image`: finales Ergebnis.
- `Recovery Confidence Matte`: Weiß zeigt bearbeitete Bereiche.
- `Clipping Map`: Rot sind Clipping-Risiken, Blau sind tiefe Schatten.
- `Recovery Difference`: zeigt die Stärke der Änderung.

### Technische Grenze

Der native Pfad erzeugt echten Float-Headroom oberhalb von 1.0 und verbessert Übergänge,
aber er kann keine ursprünglichen Sensordaten zurückholen. V2 kann zusätzlich
einen lokalen HDRTVNet++-Cache lesen. Auch das ML-Ergebnis ist eine plausible
Rekonstruktion und kein Beweis für den ursprünglichen Sensorinhalt.

### ML-Cache verwenden

1. Den Clip als nummerierte PNG- oder TIFF-Sequenz exportieren.
2. `scripts/setup_ml_backend.sh` ausführen.
3. `hdrtvnet_plus_adapter.py` auf die Sequenz anwenden.
4. Mit `radiance_cache.py build` den `.bwrcache`-Ordner erzeugen.
5. In Resolve `Use ML Cache` aktivieren.
6. Den Ordner bei `ML Cache Directory` auswählen.
7. `ML Cache Confidence` kontrollieren.
8. Bei falscher Zuordnung `Cache Frame Offset` korrigieren.

Fehlt eine Cache-Datei oder passt ihre Auflösung nicht, verwendet das Plugin
automatisch den nativen V2-Pfad.

## Temporal Integrity

Temporal Integrity reduziert AI-Flimmern, wandernde Texturen und einzelne
Helligkeitsausreißer.

Empfohlener Ablauf:

1. Mit `Temporal Error Matte` analysieren.
2. `Motion Protection Matte` prüfen.
3. Zu `Safe Repair` wechseln.
4. `Repair Strength` vorsichtig zwischen 0.2 und 0.55 einstellen.
5. Bei Doppelkonturen `Motion Guard` und `Edge Protection` erhöhen.

Das Plugin verwendet zwei vorherige und zwei folgende Frames. `Ghost Risk
Matte` zeigt Bereiche, in denen eine Reparatur Doppelkonturen erzeugen könnte.
Diese Bereiche werden durch `Ghost Guard` automatisch geschützt.

## Empfohlene Node-Reihenfolge

1. Fake Diagnostic
2. Temporal Integrity
3. AI Photorealizer
4. Radiance Recover
5. Primäre Farbkorrektur
6. Lens Physics
7. Frame Director

Die Reihenfolge ist ein Ausgangspunkt. Radiance Recover sollte vor starkem
Highlight-Clipping und Frame Director am Ende der visuellen Kette liegen.
