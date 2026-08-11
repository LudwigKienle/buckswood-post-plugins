# Buckswood Deband v1.0

## Was das Plugin macht

Buckswood Deband repariert sichtbare Tonwertstufen in Himmel, Waenden,
Schatten, unscharfen Hintergruenden, synthetischen Verlaeufen und AI-Footage.
Es ist kein globaler Blur. Der Detektor sucht auf drei Groessenskalen nach
schwachen, treppenartigen Konturen und bearbeitet nur Bereiche, die weiterhin
wie ein zusammenhaengender Verlauf aussehen.

## Empfohlener Workflow

1. Setze das Plugin moeglichst vor Grain, Sharpening und den finalen Output
   Transform.
2. Waehle das passendste Preset.
3. Stelle `View` auf `Banding Map` und erhoehe `Band Detection`, bis die
   stoerenden Konturen sichtbar werden.
4. Pruefe `Protected Detail`. Schrift, Silhouetten, Grain und Textur sollten
   cyan markiert sein.
5. Gehe zurueck auf `Result`, stelle `Repair Strength` ein und mische nur so
   viel `Gradient Dither` bei, dass die Stufen im Export nicht zurueckkommen.
6. Nutze `Output Mix` fuer die finale Dosierung.

## Parameter

- **Preset**: Waehlt abgestimmte Multiplikatoren; alle Regler bleiben als
  Feineinstellung aktiv.
- **Working Space**: `Auto / Wide Gamut` passt fuer DaVinci Intermediate, Log
  und die meisten Grading-Timelines. `Scene-Linear HDR` nur fuer echte lineare
  Lichtwerte verwenden.
- **Source Precision**: Erwartete Quantisierung. 8-bit fuer Web/H.264 oder
  generierte Quellen, 10-bit fuer Kamera-Master.
- **Repair Strength**: Staerke der rekonstruierten Verlaeufe.
- **Band Detection**: Empfindlichkeit fuer schwache False Contours.
- **Repair Radius**: Groesste analysierte Bandbreite. Ein groesserer Wert hilft
  bei breiten Himmel-Stufen, benoetigt aber mehr Rechenzeit.
- **Edge Protection**: Schuetzt harte Kanten, Typografie und Silhouetten.
- **Texture Protection**: Schuetzt Grain, Hautdetails, Haare und Stoff.
- **Chroma Repair**: Repariert zusaetzlich breite Farbstufen.
- **Gradient Dither**: Fuegt nur in erkannten Verlaeufen hochfrequentes,
  mittelwertfreies Dither hinzu.
- **Dither Motion**: `Static / Baselight Safe` ist frame-unabhaengig.
  `Frame-Indexed` veraendert sich deterministisch mit der OFX-Zeit.
- **View**: Result, Banding Map, Protected Detail oder Difference x12.
- **Output Mix**: Finaler Dry/Wet-Regler.

## Baselight

V1 arbeitet bewusst ohne vorherige Frames und ohne versteckten Temporal-Cache.
Jedes Bild entsteht ausschliesslich aus dem aktuellen Input, den Parametern und
der angeforderten OFX-Zeit. Damit bleibt das Ergebnis auch bei einer
beliebigen Render-Reihenfolge in Baselight reproduzierbar.

Typischer Linux-Installationspfad:

```text
/usr/OFX/Plugins/BuckswoodDeband.ofx.bundle
```

## ML-Status

V1 liefert noch kein ungeprueftes Forschungsmodell mit. Bei den untersuchten
Repos muessen Gewichte, Datensatz-Provenienz, Farbraum, Runtime und zeitliche
Stabilitaet getrennt validiert werden. Das Preset `AI Footage Gradient Repair`
bezeichnet deshalb das Zielmaterial, nicht einen vorgetaeuschten Neural-Modus.
