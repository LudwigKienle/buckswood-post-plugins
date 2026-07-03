# Buckswood Lens Physics Install Step by Step

## Normale Plugin-Installation fuer Giveaway/Release

Wenn du es wie ein echtes Plugin weitergeben willst, nimm die DMG:

```text
/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_Lens_Physics/release/Buckswood_Lens_Physics_Installer.dmg
```

1. DMG doppelklicken.
2. `Buckswood_Lens_Physics_Installer.pkg` doppelklicken.
3. macOS Installer durchklicken.
4. Admin-Passwort eingeben, wenn macOS fragt.
5. DaVinci Resolve neu starten.

Falls macOS meckert: Rechtsklick auf das `.pkg`, dann `Oeffnen`.

## Einfache Installation

1. DaVinci Resolve komplett schliessen.
2. Im Finder diesen Ordner oeffnen:

```text
/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_Lens_Physics
```

3. Diese Datei doppelklicken:

```text
install_buckswood_lens_physics.command
```

4. Falls macOS blockiert: Rechtsklick auf die Datei, dann `Oeffnen`.
5. Wenn Terminal nach einem Passwort fragt: Dein Mac-Admin-Passwort eingeben.
   Das Passwort wird beim Tippen nicht angezeigt, das ist normal.
6. Bei der Frage `OFX auch systemweit installieren?` zuerst `N` druecken.
7. Resolve neu starten.

## In DaVinci Resolve finden

OFX-Version:

```text
Color Page > OpenFX > Buckswood > Buckswood Lens Physics
```

DCTL-Fallback:

```text
Color Page > DCTL > Buckswood_Lens_Physics_v01
```

## Falls Resolve das OFX nicht zeigt

Den Installer nochmal starten und bei dieser Frage `y` druecken:

```text
OFX auch systemweit installieren? [y/N]
```

Dann dein Mac-Admin-Passwort eingeben und Resolve danach neu starten.

## Was der Installer macht

- baut das OFX Plugin neu
- laesst einen Smoke-Test laufen
- installiert das OFX in:

```text
/Users/ludwig.kienle/Library/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle
```

- installiert die DCTL-Fallback-Datei in:

```text
/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/Buckswood_Lens_Physics_v01.dctl
```

- leert den DaVinci OFX Plugin Cache
- bietet optional die systemweite OFX Installation an:

```text
/Library/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle
```
