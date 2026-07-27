# Buckswood DeJitter v1.1

## Zweck

Buckswood DeJitter entfernt hochfrequentes Mikro-Zittern, ohne eine gewollte
gleichmäßige Kamerafahrt vollständig festzunageln. Das Plugin analysiert eine
ausgewählte, starre Bildstelle in den vorherigen und nächsten Frames.

## Schnellstart

1. Effekt auf einen Clip oder Color-Node legen.
2. `View` auf `Tracking Region` stellen.
3. `Track Point` auf ein kontrastreiches, unbewegliches Detail setzen.
4. Den Punkt mit `Track Point Fine X/Y` pixelgenau verschieben. Optional
   `Snap Point to Texture` aktivieren.
5. `Tracking Region Width/Height` passend um dieses Detail einstellen.
6. `View` auf `Motion Vector` stellen und das Tracking prüfen.
7. Zu `Stabilized Result` wechseln und `DeJitter Strength` dosieren.

## Parameter

- `Track Point`: Mittelpunkt der analysierten Stelle.
- `Track Point Fine X/Y`: pixelgenaue Verschiebung zusätzlich zum Viewer-Griff.
- `Snap Point to Texture`: sucht in der Nähe nach einer stärkeren, starren
  Detailstruktur. Der ursprüngliche und der eingerastete Punkt werden verbunden
  angezeigt.
- `Texture Snap Radius`: maximaler Suchabstand für das Einrasten.
- `Tracking Region Width/Height`: Größe der Tracking-Maske relativ zum Bild.
- `Maximum Jitter`: Suchradius in Pixeln.
- `Temporal Analysis`: Ein Frame-Paar ist schneller; zwei Paare sind ruhiger.
- `Tracking Quality`: Genauigkeit und Rechenaufwand des Trackings.
- `DeJitter Strength`: Anteil der berechneten Korrektur.
- `Long-Term Stability`: Gewicht der zwei Frames entfernten Analyse.
- `Maximum Correction`: Sicherheitslimit gegen große Bildsprünge.
- `Tracking Confidence Guard`: Unterhalb dieser Sicherheit bleibt das Bild
  unverändert.
- `Scene Cut Protection`: Höhere Werte verwerfen unsichere Matches stärker.
- `Resampling`: Bicubic ist der Standard, Lanczos 3 ist am schärfsten.
- `Frame Edge Handling`: Auto Zoom, Spiegelung oder Randpixel-Verlängerung.
- `Output Mix`: Mischung zwischen Original und stabilisiertem Bild.

## Performance in v1.1

Die normalisierte Korrelation liest jeden Kandidaten-Patch nur noch einmal.
Unveränderte oder unsicher getrackte Frames umgehen außerdem das hochwertige
Resampling vollständig. Beide Optimierungen ändern weder die gewählte
Tracking-Qualität noch die finale Interpolation.

## Diagnoseansichten

- `Tracking Region`: zeigt die ausgewählte Region und das Tracking-Zentrum.
- `Motion Vector`: zeigt Richtung und Stärke der berechneten Korrektur.
- `Confidence`: weiß bedeutet hohe, schwarz niedrige Tracking-Sicherheit.
- `Difference`: zeigt nur die durch die Stabilisierung veränderten Bildanteile.

## Gute Tracking-Stellen

Feste Kanten, Schilder, Fensterrahmen, Architektur und texturierte Requisiten
funktionieren gut. Gesichter, Hände, Haare, Wasser, Displays, Reflexionen und
starke Bewegungsunschärfe sollten vermieden werden.
