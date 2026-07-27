# Buckswood Optics Lab v1.2

## Zweck

Optics Lab erzeugt keinen pauschalen LUT-Look. Es simuliert die Bildfehler und
Finishing-Schritte, durch die digitales oder AI-generiertes Material wie eine Aufnahme
durch ein reales Objektiv und einen realen Sensor wirkt.

## Empfohlene Reihenfolge

1. Farbraum und Primärkorrektur vor Optics Lab einstellen.
2. Ein dezentes Preset wählen, meist `AI Deplastic` oder `Large Format Clean`.
3. Focal Length, F-Stop, Sensor Width und Anamorphic Squeeze an die Szene anpassen.
4. Aberrationen nur so weit erhöhen, bis die digitale Perfektion verschwindet.
5. Eine integrierte `Glass`-Form wählen und Defocus nur bei tatsächlich unscharfen
   Bildbereichen verwenden.
6. Bloom, Diffusion und Halation zuletzt fein einstellen.
7. Sensor Grain und Output Mix im 100-Prozent-Zoom beurteilen.

## Regler

Das native OFX-Panel führt schrittweise durch die Objektiv-Anatomie: Lens State &
Focus, Distortion & Field, Chromatic Aberration, Defocus & Bokeh / Glass, Flaring &
Bloom, Vignetting, Dirt & Smudge und abschließend Sensor & Output.

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
- `Glass`: wählt eine integrierte runde, polygonale, anamorphotische, Cat-Eye-
  oder Vintage-Blendenform ohne Dateipfad.

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

### Dirt und Smudge

- `Dirt`: integrierte Staub- oder Oberflächenstruktur.
- `Smudge`: separate Fingerabdruck-, Wisch- oder Streifenstruktur.
- Beide Kanäle besitzen eigene Amount- und Scale-Regler.

## Alte lokale Glass-Assets

Der Installationsbefehl kopiert die lizenzierten Bilder nach:

```text
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

V1.2 zeigt keinen Ordner-Browser mehr. Bereits gespeicherte Pfad-, Aperture- und
Dirt-Indexwerte bleiben aus Kompatibilitätsgründen erhalten und werden verwendet,
solange die neuen Auswahllisten auf Off stehen. Die bezahlten Aperture-Dateien sind
nicht Bestandteil des öffentlichen GitHub-Releases.

## Performance in v1.2

Float32-Renderings verwenden unter macOS die Metal-Puffer von Resolve, einschließlich
der integrierten Asset-Kanäle. Byte-Renderings oder fehlende GPU-Kontexte verwenden
den CPU-Fallback im Resolve-Worker-Pool. Es gibt weder FP16 noch reduzierte Tap-Anzahl
oder Proxy-Auflösung.

## Grenzen

- Kein echter Deep-Defocus und kein separater Depth-Eingang auf der Color Page.
- Flare reagiert lokal auf Highlights und ersetzt keine vollständige Lichtquellenanalyse.
- Aperture-Images gewichten ein hochwertiges Multi-Tap-Bokeh, sind aber noch keine
  FFT-basierte Vollbild-Faltung.
- Unter Windows verwendet Optics Lab derzeit den optimierten CPU-Fallback; ein
  eigener OpenCL-Pfad ist in v1.2 noch nicht enthalten.
