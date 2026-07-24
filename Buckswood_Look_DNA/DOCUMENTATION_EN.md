# Buckswood Look DNA v2.1

## What It Does

Look DNA transfers the visual character of up to three reference stills to the
current shot. It analyzes six layers independently:

1. Tone: black point, highlight roll-off, brightness distribution, and contrast.
2. Palette: color bias, saturation, and color separation.
3. Semantic regions: broad skin, sky, foliage, warm-light, shadow, and highlight zones.
4. Texture: local contrast and fine-detail character.
5. Spatial structure: an overlapping 3x3 map prevents one global statistic from
   pushing unrelated parts of the frame in the same direction.
6. Temporal stability: cut-aware stabilization across up to five frames.

It does not reconstruct the reference scene or replace lighting. A useful
reference should have a related time of day, contrast direction, and subject
type. The plug-in then matches the look while the source shot remains itself.

## Recommended Workflow

1. Place Look DNA after technical input transforms and primary exposure balance,
   but before final grain and output sharpening.
2. Click `Browse / Load Reference A...` below `Reference A / BWLOOK`, then choose
   the main reference in the macOS or Windows file picker. Add Reference B or C
   only when they belong to the same intended look family.
3. Set `Reference Color Space` and `Footage Color Space` correctly.
4. Start with `Analysis Quality: High` and `Match Mode: Full Look`.
5. Inspect `View: Match Confidence`, then return to `Matched Result`.
6. Tune tone first, palette second, semantic matching third, and texture last.
7. Use `Original / Match Split` and lower `Overall Match` until the shot still
   feels natural in its sequence.

## Safe Starting Values

| Control | Suggested range |
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

## Controls

`Reference A / BWLOOK` is the main creative reference. Reference B and C are
optional supporting stills. Their mix sliders define the maximum contribution;
`Auto Reference Balance` then reduces a still when its exposure, contrast,
palette, or semantic distribution is incompatible with the current shot.

`Browse / Load Reference A/B/C...` opens the native file picker for JPEG, PNG,
TIFF, BMP, and `.bwlook` files. The selected local path is written to the
matching reference field and reanalyzed automatically. Nothing is uploaded to a
cloud service. If a Resolve project moves to another workstation, reselect any
reference whose local path is unavailable there.

`Spatial Look Map` blends nine overlapping regional analyses. Keep it moderate:
it should preserve local lighting relationships, not copy the reference layout.

`Shadow Transfer`, `Midtone Transfer`, and `Highlight Transfer` control where the
look is allowed to act. Lower highlight transfer for bright skies and speculars.
`Color Density` matches perceived chroma weight after palette transfer.

`Reference Color Space` tells the plug-in how to decode the still. JPEG and PNG
movie stills are usually sRGB. A frame exported through a Rec.709 Gamma 2.4
pipeline should use that setting instead.

`Footage Color Space` describes the pixels arriving at this node. With a managed
Resolve timeline, `Timeline / Display Referred` is the conservative choice. For
camera log before a color-space transform, select the matching log option.

`Match Mode` isolates Full Look, Color Only, Tone Only, or Texture Only.

`Preserve Footage Exposure` keeps the source's average exposure while allowing
the reference contrast curve to shape it.

`Skin Identity Guard` reduces hue and tone transfer on probable skin regions.
`Highlight Identity Guard` protects bright edges and specular detail.
`Scene Identity Guard` automatically lowers extreme transfers when the source
and reference distributions are too different.

`Temporal Analysis` selects Off, 3 Frames, or 5 Frames. `Temporal Look Stability`
stabilizes only measured statistics. It never blends neighboring image pixels,
so it cannot create motion trails or doubled subjects. A profile-distance check
rejects neighbors across cuts.

`Adjustment Layer Guard` avoids baking frame-specific analysis into an Resolve
adjustment layer where one analysis could otherwise affect unrelated clips.

## View Modes

`Matched Result` is the final image.

`Original / Match Split` compares source and result. Use `Split Position` to move
the divider.

`Match Difference` shows the magnitude of the transformation. Bright regions are
being changed more strongly.

`Match Confidence` is bright where source and reference statistics are compatible
and darker where the scene-identity guard is reducing the transfer.

`Skin Protection Matte` shows probable protected skin as white.

`Semantic Zones` visualizes the native zones used for selective matching:
red/orange for skin or warm light, blue for sky, green for foliage, dark gray for
shadows, and white for highlights.

`Reference Preview` displays the selected reference without changing the clip.

`Tone Mapping View` displays the tone-only contribution.

`Gamut Warning` highlights values that need strong gamut compression.

`Spatial Look Map` visualizes regional influence and confidence.

`Reference Balance` displays the effective A/B/C contribution as red/green/blue.

`Temporal Cut Guard` is green when neighboring frames are compatible and turns
red when a cut or major scene change is being rejected.

## Portable BWLOOK Profiles

For repeatable looks, batch workflows, or hosts where direct image loading is
limited, create a `.bwlook` file with `scripts/analyze_reference.py`. The profile
contains only aggregate look statistics, not the reference pixels. It can be
shared without shipping the source still.

## Limitations

- V2 uses deterministic native semantic zones, not a neural segmentation model.
- Look matching cannot invent physically correct lighting, depth, reflections,
  or missing dynamic range.
- A single still cannot describe every shot in a long sequence. Use one profile
  per lighting setup and trim `Overall Match` per shot.
- Nuke support is experimental because host temporal-frame behavior can differ.
- BWLOOK1 files contain global statistics. Direct stills are required for V2's
  spatial look map.
