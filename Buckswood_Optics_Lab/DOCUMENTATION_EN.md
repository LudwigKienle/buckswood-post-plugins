# Buckswood Optics Lab v1.3

## Purpose

Optics Lab is not a LUT. It models the optical and sensor imperfections that help
digital or AI-generated footage feel photographed rather than mathematically perfect.

## Recommended workflow

1. Set the color space and primary grade before Optics Lab.
2. Start with `AI Natural Lens`, `Clean Modern`, or the compatible `AI Deplastic`.
3. Match focal length, f-stop, sensor width, and anamorphic squeeze to the shot.
4. Add aberrations only until digital perfection starts to disappear.
5. Choose a built-in `Glass` shape and use defocus only where the image should
   genuinely be soft.
6. Balance bloom, diffusion, and halation near the end.
7. Judge sensor grain and output mix at 100 percent zoom.

## Main controls

The native OFX panel follows a lens-anatomy sequence without changing the
underlying processing order: Lens State & Focus, Distortion & Field, Chromatic
Aberration, Defocus & Bokeh / Glass, Flaring & Bloom, Vignetting, Dirt & Smudge,
then Sensor & Output and Workflow & Performance. Groups can be collapsed as the
look is completed.

### Lens state

- `Lens` selects a coherent optical recipe rather than a texture.
- `Focal Length` changes the scale of edge-dependent optical behavior.
- `F-Stop` strengthens defocus, axial CA, and coma at lower values.
- `Focus Distance` drives focus breathing.
- `Scene Units` and `Scene Scale` convert production measurements to meters without
  forcing users to rewrite shot metadata.
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
- `Iris Blades`, `Iris Roundness Trim`, and `Iris Rotation` control one procedural
  iris shared by defocus and diffraction.
- `Glass`: selects a built-in circular, polygonal, anamorphic, cat-eye, or vintage
  aperture character without any filesystem setup.

Foreground defocus now mirrors cat-eye displacement and odd-blade orientation, so
foreground bokeh no longer behaves like a copied background kernel.

Source alpha is preserved at the output. A future multi-input edition can accept a
separate depth clip without repurposing alpha.

### Finishing and sensor

- `Bloom`, `Diffusion`, and `Halation` shape highlights at different scales.
- `Flare Ghosts`, `Anamorphic Streak`, and `Starburst` add lens-light interactions.
- On v1.3 lens recipes, `Physical F-Stop Response` makes starbursts emerge near f/8
  and reach full response near f/22. Set it to zero for an unrestricted art control.
- `Sensor Debayer Character` softens red/blue detail while retaining green detail.
- `Chroma Detail Smear` reduces unnaturally perfect color resolution.
- `Sensor Grain` is temporally animated and luminance dependent.
- `Sensor ISO` scales grain energy relative to ISO 400.
- `Edge Halo Guard` reduces doubled silhouettes and hard contour halos.

### Dirt and smudge

- `Dirt`: selects a built-in dust or surface-imperfection texture.
- `Smudge`: independently selects a fingerprint, wipe, or streak texture.
- Each channel has its own Amount and Scale controls.
- All built-ins are deterministic and cached; they do not add temporal crawling.

## Workflow and performance

- `Render Quality: Full` preserves the v1.2 sampling path and is the compatibility
  default.
- `Render Quality: Preview` uses 8 rather than 12 defocus samples, 4 rather than 8
  glow samples, and 2 rather than 4 coma samples. It does not switch to FP16 or clamp
  scene-linear values.
- Seven stage switches independently bypass Geometry, Aberrations, Defocus/Iris,
  Light Effects, Vignette, Dirt/Smudge, and Sensor processing.
- A disabled stage is removed before rendering; when every stage is disabled the
  effect returns after the single source read.

The new recipes are `Clean Modern`, `Classic Spherical`, `Vintage Swirl`, `Soft
Focus Portrait`, `Anamorphic Classic 2x`, `Anamorphic Blue 1.8x`, `Vintage Flare`,
`Clinical APO`, `Rangefinder Tele`, `Retrofocus Wide`, `Modern Zoom`, and `AI Natural
Lens`. Existing preset indices 0-8 remain unchanged.

## Legacy local Glass assets

The licensed installer copies the local assets to:

```text
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

The v1.3 UI does not expose a path browser. Existing project values for the old
asset path, aperture index, and dirt index remain serialized and render unchanged
when the new selectors are Off. Paid aperture images are deliberately excluded
from the public GitHub release.

## V1.3 performance

macOS Float32 renders use Resolve's Metal buffers, including all built-in asset
channels. Safe Metal math is compared pixel-by-pixel with the CPU reference.
Byte renders and unavailable GPU contexts use the Resolve worker-pool CPU path.
Full quality remains lossless relative to v1.2. Preview changes only documented
sample counts; no path uses FP16 or clamps HDR values.

## Limitations

- No deep defocus or dedicated depth input on the Resolve Color Page.
- Flare is highlight-local and does not perform global light-source detection.
- Aperture images weight a multi-tap bokeh kernel; this is not yet a full FFT
  convolution.
- Windows currently uses the optimized CPU fallback; an OpenCL backend is not yet
  included for Optics Lab.
