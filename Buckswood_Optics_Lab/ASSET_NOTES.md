# Asset notes

## Built-in v1.2 assets

The public plug-in contains deterministic, code-baked Glass, Dirt, and Smudge
textures. They are generated once in memory and cached. No file picker, filesystem
path, download, or third-party image is required for these selectors.

`Lens` remains an algorithmic recipe selector. It does not select an image: each
recipe combines geometry, aberration, focus, highlight, sensor, and warmth
coefficients. `Glass` weights the bokeh aperture kernel. `Dirt` and `Smudge` are
separate transmission/highlight-scatter texture channels.

## Licensed Glass aperture library

The 157 aperture JPG files are paid packaged content from the user's local Glass
installation. They remain supported by hidden legacy path/index parameters for
backward-compatible saved projects, but are not stored in this repository or copied
into public builds.

Expected layout:

```text
GlassAssets/
  apertures/aperture001.jpg ... aperture157.jpg
  dirt_textures/*.png
```

## CC0 dirt textures

The supported `acg_*` dirt maps originate from ambientCG and are published under the
Creative Commons CC0 1.0 Universal public-domain dedication:

- Fingerprints001, Fingerprints002, Fingerprints003
- SurfaceImperfections001 through SurfaceImperfections005

Source: <https://ambientcg.com>

They may legally be redistributed, but this repository still loads the user's local
copies so that the public source tree stays lightweight.

## Image decoder

`third_party/stb_image.h` is `stb_image` v2.30 from the nothings/stb project:
<https://github.com/nothings/stb>

It is available under the public-domain dedication or MIT license included at the end
of that header.
