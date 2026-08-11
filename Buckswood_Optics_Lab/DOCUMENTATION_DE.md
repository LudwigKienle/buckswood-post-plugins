# Buckswood Optics Lab v1.3.1

## Zweck

Optics Lab erzeugt keinen pauschalen LUT-Look. Es simuliert die Bildfehler und
Finishing-Schritte, durch die digitales oder AI-generiertes Material wie eine Aufnahme
durch ein reales Objektiv und einen realen Sensor wirkt.

## Empfohlene Reihenfolge

1. Farbraum und Primärkorrektur vor Optics Lab einstellen.
2. Mit `AI Natural Lens`, `Clean Modern` oder dem kompatiblen `AI Deplastic` starten.
3. Focal Length, F-Stop, Sensor Width und Anamorphic Squeeze an die Szene anpassen.
4. Aberrationen nur so weit erhöhen, bis die digitale Perfektion verschwindet.
5. Eine integrierte `Glass`-Form wählen und Defocus nur bei tatsächlich unscharfen
   Bildbereichen verwenden.
6. Bloom, Diffusion und Halation zuletzt fein einstellen.
7. Sensor Grain und Output Mix im 100-Prozent-Zoom beurteilen.

## Regler

Das native OFX-Panel führt schrittweise durch die Objektiv-Anatomie: Lens State &
Focus, Distortion & Field, Chromatic Aberration, Defocus & Bokeh / Glass, Flaring &
Bloom, Vignetting, Dirt & Smudge, Sensor & Output sowie Workflow & Performance.

### Lens State

- `Focal Length`: beeinflusst die Stärke randabhängiger Abbildungsfehler.
- `F-Stop`: kleine Werte verstärken Defocus, Axial CA und Coma.
- `Focus Distance`: steuert den Focus-Breathing-Faktor.
- `Scene Units` und `Scene Scale`: rechnen Set-Maße korrekt in Meter um.
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
- `Iris Blades`, `Iris Roundness Trim` und `Iris Rotation`: steuern eine gemeinsame
  prozedurale Blende für Defocus und Beugungsstrahlen.
- `Glass`: wählt eine integrierte runde, polygonale, anamorphotische, Cat-Eye-
  oder Vintage-Blendenform ohne Dateipfad.

Wichtig: Wird Alpha als Depth verwendet, bleibt das ursprüngliche Alpha am Ausgang
erhalten. Für eine separate Depth Map ist später eine Multi-Input-Version sinnvoll.
Vordergrund-Defocus spiegelt nun Cat-Eye-Versatz und ungerade Blendenformen korrekt.

### Finishing und Sensor

- `Bloom`: additiver Highlight-Glow.
- `Diffusion`: weichere, breitere Highlight-Verteilung.
- `Halation`: rotwarmer Saum um helle Bereiche.
- `Flare Ghosts`: spiegelbildliche interne Reflexion.
- `Anamorphic Streak`: horizontaler blauer Lichtstreifen.
- `Starburst`: blendenabhängige Beugung um sehr helle Quellen.
- `Physical F-Stop Response`: lässt Starbursts bei den neuen Presets etwa ab f/8
  erscheinen und bis f/22 anwachsen. Null deaktiviert diese physikalische Kopplung.
- `Sensor Debayer Character`: weichere Rot-/Blau-Details bei schärferem Grün.
- `Chroma Detail Smear`: reduziert unnatürlich perfekte Farbauflösung.
- `Sensor Grain`: zeitlich animiertes, helligkeitsabhängiges Korn.
- `Sensor ISO`: skaliert die Kornenergie relativ zu ISO 400.
- `Edge Halo Guard`: reduziert Doppelkonturen an harten Silhouetten.

### Dirt und Smudge

- `Dirt`: integrierte Staub- oder Oberflächenstruktur.
- `Smudge`: separate Fingerabdruck-, Wisch- oder Streifenstruktur.
- Beide Kanäle besitzen eigene Amount- und Scale-Regler.

## Workflow und Performance

- `Render Quality: Full` erhält den bisherigen v1.2-Samplingpfad und bleibt Standard.
- `Preview` verwendet 8 statt 12 Defocus-, 4 statt 8 Glow- und 2 statt 4 Coma-Samples.
  HDR-Werte bleiben Float32 und werden nicht beschnitten.
- Sieben Stage-Schalter deaktivieren Geometry, Aberrations, Defocus/Iris, Light,
  Vignette, Dirt/Smudge und Sensor unabhängig voneinander.
- Deaktivierte Stufen werden vor dem Rendern entfernt. Sind alle aus, benötigt der
  Effekt nur einen Quellpixel-Zugriff.

Neu sind `Clean Modern`, `Classic Spherical`, `Vintage Swirl`, `Soft Focus Portrait`,
`Anamorphic Classic 2x`, `Anamorphic Blue 1.8x`, `Vintage Flare`, `Clinical APO`,
`Rangefinder Tele`, `Retrofocus Wide`, `Modern Zoom` und `AI Natural Lens`. Die alten
Preset-Indizes 0 bis 8 bleiben unverändert.

## Alte lokale Glass-Assets

Der Installationsbefehl kopiert die lizenzierten Bilder nach:

```text
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

V1.3 zeigt keinen Ordner-Browser. Bereits gespeicherte Pfad-, Aperture- und
Dirt-Indexwerte bleiben aus Kompatibilitätsgründen erhalten und werden verwendet,
solange die neuen Auswahllisten auf Off stehen. Die bezahlten Aperture-Dateien sind
nicht Bestandteil des öffentlichen GitHub-Releases.

## Performance in v1.3.1

Float32-Renderings verwenden unter macOS die Metal-Puffer von Resolve, einschließlich
der integrierten Asset-Kanäle. Byte-Renderings oder fehlende GPU-Kontexte verwenden
den CPU-Fallback im Resolve-Worker-Pool. Full bleibt verlustfrei kompatibel; Preview
ändert ausschließlich die dokumentierten Sample-Anzahlen. FP16 wird nicht verwendet.

## Grenzen

- Kein echter Deep-Defocus und kein separater Depth-Eingang auf der Color Page.
- Flare reagiert lokal auf Highlights und ersetzt keine vollständige Lichtquellenanalyse.
- Aperture-Images gewichten ein hochwertiges Multi-Tap-Bokeh, sind aber noch keine
  FFT-basierte Vollbild-Faltung.
- Unter Windows verwendet Optics Lab derzeit den optimierten CPU-Fallback; ein
  eigener OpenCL-Pfad ist in v1.3 noch nicht enthalten.
