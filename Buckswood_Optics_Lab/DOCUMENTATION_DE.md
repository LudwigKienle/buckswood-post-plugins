# Buckswood Optics Lab v1.1

## Zweck

Optics Lab erzeugt keinen pauschalen LUT-Look. Es simuliert die Bildfehler und
Finishing-Schritte, durch die digitales oder AI-generiertes Material wie eine Aufnahme
durch ein reales Objektiv und einen realen Sensor wirkt.

## Empfohlene Reihenfolge

1. Farbraum und Primärkorrektur vor Optics Lab einstellen.
2. Ein dezentes Preset wählen, meist `AI Deplastic` oder `Large Format Clean`.
3. Focal Length, F-Stop, Sensor Width und Anamorphic Squeeze an die Szene anpassen.
4. Aberrationen nur so weit erhöhen, bis die digitale Perfektion verschwindet.
5. Defocus und Glass Aperture nur bei tatsächlich unscharfen Bildbereichen verwenden.
6. Bloom, Diffusion und Halation zuletzt fein einstellen.
7. Sensor Grain und Output Mix im 100-Prozent-Zoom beurteilen.

## Regler

### Lens State

- `Focal Length`: beeinflusst die Stärke randabhängiger Abbildungsfehler.
- `F-Stop`: kleine Werte verstärken Defocus, Axial CA und Coma.
- `Focus Distance`: steuert den Focus-Breathing-Faktor.
- `Sensor Width`: verändert die relative Bildkreis-/Fokalwirkung.
- `Anamorphic Squeeze`: formt Bokeh und Streak horizontal.
- `Anamorphic Axis`: dreht Bokeh-Ellipse und Streak passend zur Objektivachse.

### Aberrations

- `Distortion Trim`: zusätzliche Tonnen- oder Kissenverzeichnung.
- `Lateral CA`: radiale RGB-Aufspaltung zum Bildrand.
- `Axial CA`: farbabhängige Unschärfe bei offener Blende.
- `Coma`: zieht helle Punkte am Rand kometenförmig.
- `Astigmatism`: trennt radiale und tangentiale Schärfe.
- `Field Curvature`: reduziert die Randfokussierung.
- `Spherical Aberration`: weicher, leicht leuchtender Fokus.
- `Swirl`: rotierende Randcharakteristik.

### Defocus

- `Uniform Focus Offset`: globale Test-/Look-Unschärfe.
- `Source Alpha as Depth`: interpretiert Alpha als 0-bis-1-Tiefenkanal.
- `Invert Alpha Depth`: kehrt Vorder- und Hintergrund der Depth Map um.
- `Alpha Depth Near/Far`: kalibriert den tatsächlich genutzten Wertebereich.
- `Alpha Depth Gamma`: verteilt die Fokusabstände innerhalb der Depth Map.
- `Alpha Focus Plane`: die Alpha-Tiefe, die scharf bleiben soll.
- `Cat-Eye Bokeh`: beschneidet Bokeh zum Bildrand.
- `Glass Aperture Index`: 1 bis 157 lädt das entsprechende lokale Aperture-JPG.

Wichtig: Wird Alpha als Depth verwendet, bleibt das ursprüngliche Alpha am Ausgang
erhalten. Für eine separate Depth Map ist später eine Multi-Input-Version sinnvoll.

### Finishing und Sensor

- `Bloom`: additiver Highlight-Glow.
- `Diffusion`: weichere, breitere Highlight-Verteilung.
- `Halation`: rotwarmer Saum um helle Bereiche.
- `Flare Ghosts`: spiegelbildliche interne Reflexion.
- `Anamorphic Streak`: horizontaler blauer Lichtstreifen.
- `Starburst`: kreuzförmige Beugung um sehr helle Quellen.
- `Sensor Debayer Character`: weichere Rot-/Blau-Details bei schärferem Grün.
- `Chroma Detail Smear`: reduziert unnatürlich perfekte Farbauflösung.
- `Sensor Grain`: zeitlich animiertes, helligkeitsabhängiges Korn.
- `Sensor ISO`: skaliert die Kornenergie relativ zu ISO 400.
- `Edge Halo Guard`: reduziert Doppelkonturen an harten Silhouetten.

## Lokale Glass-Assets

Der Installationsbefehl kopiert die lizenzierten Bilder nach:

```text
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

Mit dem Ordner-Browser kann auch direkt der ursprüngliche `glass`-Ordner gewählt
werden. Die bezahlten Aperture-Dateien sind nicht Bestandteil des öffentlichen
GitHub-Releases.

## Performance in v1.1

Inaktive Stufen führen keine unnötigen Nachbarschafts-, CA- oder Mapping-Samples
mehr aus. Der neutrale Pfad benötigt nur den Originalpixel. Assets bleiben
dekodiert im Cache; die finale Bildqualität und Tap-Anzahl aktiver Effekte wurden
nicht reduziert.

## Grenzen

- Kein echter Deep-Defocus und kein separater Depth-Eingang auf der Color Page.
- Flare reagiert lokal auf Highlights und ersetzt keine vollständige Lichtquellenanalyse.
- Aperture-Images gewichten ein hochwertiges Multi-Tap-Bokeh, sind aber noch keine
  FFT-basierte Vollbild-Faltung.
- GPU-Backends folgen nach der visuellen Validierung des CPU-Referenzpfads.
