# Buckswood Film Emulation v2.1

`Buckswood Film Emulation` ist ein natives OpenFX Film-Finishing-Plugin fuer DaVinci Resolve. Es ist fuer zwei Faelle gebaut: echtes Kameramaterial mit kontrolliertem Negativ/Print-Finish, und AI-Footage, das weniger plastisch, weniger digital und fotografischer wirken soll.

Es ist keine gemessene Kopie von Dehancer oder Genesis. V2 verfolgt einen eigenen Ansatz: ein offenes Buckswood-Tool, das Filmprozess-Regler mit AI-Footage-Reparatur verbindet.

## Empfohlene Node-Reihenfolge

```text
1. Technische Korrektur / CST / Base Balance
2. Buckswood Photorealizer oder Lens Physics, falls gebraucht
3. Buckswood Film Emulation
4. Shot-Matching / kleine Secondaries
```

Wenn du Log-Material direkt in das Plugin gibst, waehle den passenden `Input Space`. Wenn dein Material bereits Rec.709 ist, nimm `Rec.709 / Gamma 2.4` oder bei generativer Footage `AI Footage Rec.709`.

## V2 Pipeline

Intern arbeitet das Plugin in dieser Reihenfolge:

```text
Input Decode
Temporal Reconstruction
Exposure / Push-Pull / Film Breath
Film Developer
Negative Stock
Film Compression
Halation / Bloom
Printer Lights
Print Stock
Expand / Rolloff
Grain / Damage / Gate Weave
Output Encode
```

Das ist wichtig: Die Regler sind als Prozess-Stufen gedacht, nicht als zufaellige Look-Slider.

## Wichtigste Regler

`Input Space` bestimmt, wie das Plugin die Bildwerte intern interpretiert.

`Film / AI Stock` ist der Hauptlook. Die AI-Presets sind bewusst korrektiv:

- `AI Footage Deplastic Film`: Standard fuer generative Clips.
- `AI Skin Recovery Negative`: vorsichtiger fuer Gesichter und Haut.
- `AI Dream Grain`: gibt sehr cleanen AI-Bildern Textur und Bewegung.
- `AI Clean Commercial Film`: sauberer Look fuer Produkt, Fashion, Architektur und Werbung.

`Print Stock` ist die zweite Bildformungsstufe. Fuer AI-Footage ist `AI Soft Print` meistens der beste Startpunkt.

`Process Mode` bestimmt, welche Ebene ausgegeben wird:

- `Full Process`: Farbe plus optische Textur.
- `Color Only`: Developer, Stock, Compression, Printer Lights und Print ohne Grain/Halation/Damage.
- `Texture Only`: Halation, Bloom, Grain, Dust, Scratches, Gate Weave und Breath ohne Print-Farbkurven.

`Push / Pull` verschiebt Exposure und Entwicklungsenergie. Positive Werte wirken dichter und stressiger, negative Werte offener und weicher.

`Film Compression` macht harte digitale Highlights weicher. Diesen Regler zuerst nutzen, bevor Bloom oder Halation stark erhoeht werden.

`Printer Cyan / Magenta / Yellow` ist der analoge Farbtrim. Kleine Bewegungen reichen meistens.

`Subtractive Color` macht Farben dichter und weniger videoartig. Fuer AI-Footage sind 0.25 bis 0.45 oft genug.

`Halation` und `Bloom` geben Lichtquellen analoges Verhalten. Fuer realistische Ergebnisse niedrig halten.

`Grain Amount`, `Grain Profile` und `Grain Chroma` steuern die Filmtextur. AI-Footage profitiert meistens von feinem Korn statt starkem Korn.

`Temporal AI Reconstruction` mischt Nachbarframes mit Motion Guard. Der Regler ist standardmaessig aus, weil Multi-Frame-Rendering schwerer ist. Nutze 0.15 bis 0.35, wenn AI-Footage flimmert, schimmert oder instabile Mikrotextur zeigt.

`Skin Protect` schuetzt Hautfarben vor zu starker Temperatur-, Tint-, Subtractive-Color- und Color-Separation-Verschiebung.

`Output Mix` ist der Sicherheitsregler. Wenn der Look gut ist, aber zu stark wirkt, zuerst Mix senken.

## Diagnose-Ansichten

- `Final Image`: normales Ergebnis.
- `Grain Matte`: zeigt die erzeugte Kornstruktur.
- `Halation / Bloom Matte`: zeigt, wo der optische Glow arbeitet.
- `Density Map`: zeigt, wie stark Dichte und Print-Stufe wirken.
- `Temporal Reconstruction Map`: Blau bedeutet kaum temporale Aenderung; helle Werte zeigen Bereiche, in denen Temporal Reconstruction eingreift.

## Gute Startwerte Fuer AI-Footage

```text
Input Space: AI Footage Rec.709
Film / AI Stock: AI Footage Deplastic Film
Print Stock: AI Soft Print
Subtractive Color: 0.34
Film Compression: 0.38
Highlight Rolloff: 0.62
Halation: 0.08-0.16
Grain Amount: 0.12-0.24
Temporal AI Reconstruction: 0.00 zuerst, dann 0.15-0.35 bei sichtbarem Shimmer
Skin Protect: 0.70
Output Mix: 0.65-0.80
```

## Warum Es Sich Abhebt

Klassische Film-Emulation verkauft oft viele gemessene Filmidentitaeten. Buckswood Film Emulation V2 ist staerker auf moderne Problemfootage ausgelegt: generative Clips, AI-Shimmer, wachsige Haut, harte digitale Highlights, zu saubere Textur und plastische Farben.

Fuer tiefere temporale Reparatur nutze das Companion-Skript `scripts/temporal_ml_reconstruct.py` vor oder nach Resolve. Es kann eine leichte Fallback-Methode nutzen oder ein externes BasicVSR++-aehnliches Backend aufrufen.
