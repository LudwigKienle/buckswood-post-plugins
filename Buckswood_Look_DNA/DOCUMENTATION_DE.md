# Buckswood Look DNA v2.1

## Was Das Plugin Macht

Look DNA uebertraegt die visuelle Charakteristik von bis zu drei Referenzstills
auf den aktuellen Shot. Dabei werden sechs Ebenen getrennt analysiert:

1. Tonwerte: Schwarzpunkt, Highlight-Roll-off, Helligkeitsverteilung und Kontrast.
2. Farbpalette: Farbrichtung, Saettigung und Farbtrennung.
3. Semantische Bereiche: Haut, Himmel, Vegetation, warmes Licht, Schatten und Highlights.
4. Textur: lokaler Kontrast und feine Detailstruktur.
5. Raeumliche Struktur: Eine ueberlappende 3x3-Map verhindert unpassende globale Verschiebungen.
6. Zeitliche Stabilitaet: schnittgeschuetzte Analyse ueber bis zu fuenf Frames.

Das Plugin kopiert keine Personen, Objekte oder Beleuchtung aus dem Referenzbild.
Am besten funktioniert eine Referenz mit aehnlicher Tageszeit, Kontrastrichtung
und Motivart. Der Shot behaelt dadurch seine Identitaet.

## Empfohlener Ablauf

1. Setze Look DNA nach technischem Input-Transform und primaerer
   Belichtungskorrektur, aber vor finalem Grain und Output-Sharpening.
2. Klicke unter `Reference A / BWLOOK` auf
   `Browse / Load Reference A...` und waehle die Hauptreferenz im macOS- oder
   Windows-Dateidialog. Reference B und C sind optional und sollten zur gleichen
   Look-Familie gehoeren.
3. Stelle Referenz- und Footage-Farbraum korrekt ein.
4. Starte mit `Analysis Quality: High` und `Match Mode: Full Look`.
5. Pruefe zuerst `View: Match Confidence` und gehe dann zu `Matched Result`.
6. Passe zuerst Tone, dann Palette, Semantic Match und zuletzt Texture an.
7. Nutze den Split und reduziere `Overall Match`, bis der Shot in der Sequenz
   weiterhin natuerlich wirkt.

## Sichere Startwerte

| Regler | Empfohlener Bereich |
| --- | --- |
| Overall Match | 0.65-0.85 |
| Tone & Contrast | 0.65-0.80 |
| Palette & Color Separation | 0.55-0.75 |
| Semantic Region Match | 0.40-0.65 |
| Local Contrast Match | 0.20-0.45 |
| Texture Match | 0.10-0.35 |
| Reference Grain | 0.00-0.20 |
| Auto Reference Balance | 0.60-0.85 |
| Spatial Look Map | 0.20-0.45 |
| Shadow / Midtone / Highlight Transfer | 0.75 / 1.00 / 0.65 |
| Color Density | 0.30-0.55 |
| Skin Identity Guard | 0.70-0.90 |
| Highlight Identity Guard | 0.70-0.90 |
| Scene Identity Guard | 0.65-0.85 |
| Temporal Look Stability | 0.50-0.75 |
| Gamut Guard | 0.80-1.00 |

## Wichtige Regler

`Reference Color Space` beschreibt den Farbraum des Stills. JPEG- und PNG-Stills
sind meistens sRGB. Ein aus einer Rec.709-Gamma-2.4-Timeline exportierter Frame
sollte entsprechend als Rec.709 Gamma 2.4 eingestellt werden.

`Footage Color Space` beschreibt die Pixel, die an diesem Node ankommen. In einer
farbverwalteten Resolve-Timeline ist `Timeline / Display Referred` ein guter
Startpunkt. Vor einem CST kann der passende Kamera-Log-Farbraum gewaehlt werden.

`Reference A` ist die kreative Hauptreferenz. B und C sind optionale
Ergaenzungen. Die Mix-Regler bestimmen ihren Maximalanteil. `Auto Reference
Balance` reduziert automatisch eine Referenz, deren Belichtung, Kontrast,
Palette oder Motive statistisch schlecht zum aktuellen Shot passen.

`Browse / Load Reference A/B/C...` oeffnet den nativen Dateidialog. Unterstuetzt
werden JPEG, PNG, TIFF, BMP und `.bwlook`. Der ausgewaehlte lokale Pfad wird
automatisch in das zugehoerige Referenzfeld geschrieben und sofort neu
analysiert. Es findet kein Cloud-Upload statt. Wird ein Resolve-Projekt auf
einen anderen Rechner verschoben, muss eine dort fehlende Referenz neu
ausgewaehlt werden.

`Spatial Look Map` mischt neun ueberlappende Regionen weich. Moderate Werte
erhalten lokale Lichtverhaeltnisse, ohne das Layout der Referenz zu kopieren.

`Shadow Transfer`, `Midtone Transfer` und `Highlight Transfer` bestimmen, in
welchen Helligkeitsbereichen der Look wirken darf. `Color Density` passt das
wahrgenommene Farbgewicht nach dem Palette-Match an.

`Preserve Footage Exposure` erhaelt die mittlere Belichtung des Originals und
uebertraegt vor allem die Form der Referenz-Kontrastkurve.

`Skin Identity Guard`, `Highlight Identity Guard` und `Scene Identity Guard`
verhindern, dass Haut, helle Kanten oder die gesamte Shot-Identitaet zu stark an
eine unpassende Referenz angepasst werden.

`Temporal Analysis` waehlt Off, 3 Frames oder 5 Frames. `Temporal Look Stability`
glaettet nur Analysewerte. Es werden keine Pixel aus Nachbarframes gemischt.
Dadurch entstehen keine Doppelkonturen oder Geisterbilder. Ein Distanztest trennt
unterschiedliche Shots an Schnitten.

`Adjustment Layer Guard` verhindert eine clipabhaengige Analyse auf Resolve-
Adjustment-Layern, die mehrere unterschiedliche Clips ueberdecken.

## Ansichten

`Matched Result`: finales Ergebnis.

`Original / Match Split`: Original und Match nebeneinander; Position ueber
`Split Position`.

`Match Difference`: helle Stellen werden staerker veraendert.

`Match Confidence`: helle Stellen besitzen eine hohe statistische Uebereinstimmung;
dunklere Stellen werden vom Scene Identity Guard staerker geschuetzt.

`Skin Protection Matte`: erkannte und geschuetzte Hautbereiche erscheinen weiss.

`Semantic Zones`: Rot/Orange zeigt Haut oder warmes Licht, Blau Himmel, Gruen
Vegetation, Dunkelgrau Schatten und Weiss Highlights.

`Reference Preview`: zeigt das geladene Referenzbild.

`Tone Mapping View`: zeigt nur den Tonwertanteil des Matches.

`Gamut Warning`: markiert Werte, die stark komprimiert werden muessen.

`Spatial Look Map`: visualisiert regionalen Einfluss und Match-Confidence.

`Reference Balance`: zeigt den effektiven A/B/C-Anteil als Rot/Gruen/Blau.

`Temporal Cut Guard`: Gruen bedeutet kompatible Nachbarframes; Rot zeigt einen
erkannten Schnitt oder starken Szenenwechsel.

## BWLOOK-Profile

Mit `scripts/analyze_reference.py` kann aus einem Still eine kleine `.bwlook`-
Datei erzeugt werden. Sie enthaelt nur zusammengefasste Look-Statistiken und keine
Bildpixel. Das eignet sich fuer wiederholbare Looks, Batch-Workflows und Nuke.

## Grenzen Von V2

- V2 verwendet schnelle, deterministische semantische Zonen und kein neuronales
  Segmentierungsmodell.
- Look Matching erzeugt keine physikalisch korrekte Beleuchtung, Tiefe,
  Reflexionen oder fehlenden Dynamikumfang.
- Fuer lange Sequenzen sollte pro Lichtsetup ein Profil verwendet und die Staerke
  pro Shot angepasst werden.
- Nuke-Unterstuetzung ist experimentell, da Hosts zeitliche Frames unterschiedlich liefern.
- BWLOOK1-Dateien enthalten globale Statistiken. Fuer die 3x3-Spatial-Map muss
  ein direktes Referenzbild geladen werden.
