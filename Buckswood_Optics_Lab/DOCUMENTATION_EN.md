# Buckswood Optics Lab v1.1

## Purpose

Optics Lab is not a LUT. It models the optical and sensor imperfections that help
digital or AI-generated footage feel photographed rather than mathematically perfect.

## Recommended workflow

1. Set the color space and primary grade before Optics Lab.
2. Start with `AI Deplastic` or `Large Format Clean`.
3. Match focal length, f-stop, sensor width, and anamorphic squeeze to the shot.
4. Add aberrations only until digital perfection starts to disappear.
5. Use defocus and a Glass aperture only where the image should genuinely be soft.
6. Balance bloom, diffusion, and halation near the end.
7. Judge sensor grain and output mix at 100 percent zoom.

## Main controls

### Lens state

- `Focal Length` changes the scale of edge-dependent optical behavior.
- `F-Stop` strengthens defocus, axial CA, and coma at lower values.
- `Focus Distance` drives focus breathing.
- `Sensor Width` changes the relationship between image circle and focal length.
- `Anamorphic Squeeze` shapes bokeh and horizontal streaks.
- `Anamorphic Axis` rotates the bokeh ellipse and streak direction.

### Aberrations

- `Distortion Trim`: additional barrel or pincushion distortion.
- `Lateral CA`: radial RGB separation toward the frame edge.
- `Axial CA`: color-dependent blur at wide apertures.
- `Coma`: comet-shaped highlight deformation near the edge.
- `Astigmatism`: separates radial and tangential focus.
- `Field Curvature`: softens the image plane toward the edge.
- `Spherical Aberration`: soft, slightly glowing focus.
- `Swirl`: rotational edge character.

### Defocus

- `Uniform Focus Offset`: global look-development blur.
- `Source Alpha as Depth`: interprets alpha as a normalized depth channel.
- `Invert Alpha Depth`: swaps foreground and background depth direction.
- `Alpha Depth Near/Far`: calibrates the useful range of the depth channel.
- `Alpha Depth Gamma`: redistributes focus distances within that range.
- `Alpha Focus Plane`: alpha depth that remains in focus.
- `Cat-Eye Bokeh`: clips bokeh toward the edge of frame.
- `Glass Aperture Index`: values 1 through 157 load the matching local aperture JPG.

Source alpha is preserved at the output. A future multi-input edition can accept a
separate depth clip without repurposing alpha.

### Finishing and sensor

- `Bloom`, `Diffusion`, and `Halation` shape highlights at different scales.
- `Flare Ghosts`, `Anamorphic Streak`, and `Starburst` add lens-light interactions.
- `Sensor Debayer Character` softens red/blue detail while retaining green detail.
- `Chroma Detail Smear` reduces unnaturally perfect color resolution.
- `Sensor Grain` is temporally animated and luminance dependent.
- `Sensor ISO` scales grain energy relative to ISO 400.
- `Edge Halo Guard` reduces doubled silhouettes and hard contour halos.

## Local Glass assets

The licensed installer copies the local assets to:

```text
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

The directory browser can also point directly at an existing `glass` folder. Paid
aperture images are deliberately excluded from the public GitHub release.

## V1.1 performance

Inactive stages no longer perform unnecessary neighborhood, CA, or mapping
samples. The neutral path reads only the source pixel, while active effects keep
their full-quality sample counts. Decoded local assets remain cached.

## Limitations

- No deep defocus or dedicated depth input on the Resolve Color Page.
- Flare is highlight-local and does not perform global light-source detection.
- Aperture images weight a multi-tap bokeh kernel; this is not yet a full FFT
  convolution.
- GPU backends follow after visual validation of the CPU reference implementation.
