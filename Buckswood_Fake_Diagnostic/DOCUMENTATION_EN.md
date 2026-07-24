# Buckswood Fake Diagnostic

English documentation for DaVinci Resolve

Version: 2.2.0
Plugin type: OpenFX / OFX  
Host: DaVinci Resolve  
Resolve category: `Buckswood`  
Plugin name: `Buckswood Fake Diagnostic`

## Short Description

`Buckswood Fake Diagnostic` is a diagnostic and assist tool for AI footage, CG elements, and compositing shots that often receive the review note:

> It feels a bit fake.

The plugin treats that note as a diagnostic problem, not as a vague creative opinion. It looks for common visual signals that can make an image or element feel artificial:

- overly smooth or plastic surfaces
- hard or digital-looking highlights
- overly sharp edges or matte issues
- saturation, gamut, or grade mismatch
- missing sensor texture and micro detail
- missing or mismatched temporal movement between frames

The plugin is intentionally not a one-click look. Its main purpose is to help identify why a shot feels fake.

## Installation

Recommended installation:

1. Quit DaVinci Resolve completely.
2. Open `Buckswood_Fake_Diagnostic_Installer.dmg`.
3. Run `Buckswood_Fake_Diagnostic_Installer.pkg`.
4. Finish the macOS Installer steps.
5. Restart DaVinci Resolve.

Installed path:

```text
/Library/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle
```

In Resolve, find the plugin here:

```text
Color Page > OpenFX > Buckswood > Buckswood Fake Diagnostic
```

## Core Idea

The plugin analyzes each pixel together with its neighboring pixels. In v2, it can also read the previous and next frame to detect temporal problems. From this image context, it calculates several internal scores. Each score represents a likely cause of a "fake" feeling:

- `Plastic/Smoothness`
- `Highlight/Clip`
- `Edge/Matte`
- `Grade/Gamut`
- `Texture`
- `Temporal Motion/Stability`

These individual scores are combined into an overall problem score. This score indicates how strongly a given image region matches common fake-looking image patterns.

## View Modes

### Diagnostic Overlay

This mode overlays diagnostic colors on top of the original image.

Useful for:

- quick review analysis
- locating suspicious image areas
- first-pass diagnosis before applying correction

### Problem Matte

This mode displays a black-and-white suspicion matte.

Interpretation:

- black: low risk
- white: high risk

Useful for:

- checking whether the plugin is detecting the right areas
- creating a diagnostic matte
- tuning sensitivity and category weights

### Reality Match Assist

This mode does not show diagnostic colors. Instead, it applies subtle corrective processing.

It can:

- restore micro contrast
- slightly soften harsh edges
- roll off highlights more gently
- reduce extreme saturation or gamut pressure
- add light sensor texture

Useful for:

- quick look tests
- making AI footage feel less plastic
- finding a direction for the final grade

### Assist + Overlay

This mode combines correction and diagnostic overlay.

Useful for:

- seeing what is being corrected
- technical communication with compositing or grading
- before/after evaluation with diagnostic feedback

### Category Colors

This mode displays only the dominant problem category as false color.

Colors:

- magenta: plastic / overly smooth
- yellow: highlights / clipping
- cyan: edges / matte / edge issues
- red: grade / gamut / saturation
- blue: missing texture
- green: temporal / movement / flicker

Useful for:

- fast root-cause analysis
- VFX supervision
- explaining the issue to a director, compositor, or colorist

### Temporal Stability Matte

This mode shows only the temporal risk score as a black-and-white matte.

Useful for:

- finding areas that feel too static or too flickery
- checking AI footage for missing camera micro-motion
- checking CG elements for mismatched frame-to-frame stability

### Temporal Difference Overlay

This mode shows frame-to-frame difference as a colored overlay.

Useful for:

- spotting motion-jitter problems faster
- revealing flicker caused by grade, highlights, or AI artifacts
- checking whether temporal behavior is actually the main issue before using assist mode

## Parameters

### View Mode

Selects the output view:

- `Diagnostic Overlay`
- `Problem Matte`
- `Reality Match Assist`
- `Assist + Overlay`
- `Category Colors`
- `Temporal Stability Matte`
- `Temporal Difference Overlay`

### Sensitivity

Controls how strongly the plugin reacts to fake-looking signals.

Suggested ranges:

- `0.5 - 0.8`: conservative
- `1.0`: default
- `1.5 - 3.0`: aggressive diagnosis

### Overlay Strength

Controls how strongly the diagnostic colors are mixed over the image.

Relevant for:

- `Diagnostic Overlay`
- `Assist + Overlay`

### Correction Strength

Controls the strength of the automatic assist correction.

Relevant for:

- `Reality Match Assist`
- `Assist + Overlay`

Suggested ranges:

- `0.10 - 0.25`: subtle
- `0.35`: default
- `0.50+`: clearly visible

### Temporal Analysis

Enables comparison with neighboring frames.

Recommendation:

- keep it enabled for AI, CG, and compositing shots
- turn it off if a specific host setup does not provide neighboring frames reliably

### Temporal Frame Offset

Selects how far the plugin looks backward and forward in time.

Recommendation:

- `1 Frame`: default for diagnosis and assist
- `2 Frames`: slightly wider movement comparison
- `3 Frames`: only for slow or very stable material

### Plastic/Smooth Weight

Controls how strongly overly smooth or plastic-looking areas are weighted.

Increase this when:

- AI skin looks too smooth
- surfaces feel too clean
- fine detail is missing

### Highlight/Clip Weight

Controls how strongly hard highlights and clipping are detected.

Increase this when:

- bright areas look digital
- highlights do not roll off naturally
- CG lights feel too clean or flat

### Edge/Matte Weight

Controls how strongly harsh edges are evaluated.

Increase this when:

- composites feel cut out
- edges are too sharp
- matte issues are suspected

### Grade/Gamut Weight

Controls how strongly saturation, gamut, and grade mismatch risks are evaluated.

Increase this when:

- an element does not match the plate color
- a new LUT or grade has broken the VFX layer
- colors feel too video-like

### Texture Weight

Controls how strongly missing micro texture is evaluated.

Increase this when:

- AI footage looks too clean
- sensor noise is missing
- the image does not feel like real camera footage

### Temporal Weight

Controls how strongly temporal problems contribute to the overall score.

Increase this when:

- AI footage feels too stable
- CG elements do not inherit the plate's micro-motion
- highlights or grade artifacts flicker across frames

### Micro Contrast Assist

Adds subtle micro contrast back into the image in assist modes.

Useful for:

- AI footage
- overly soft renders
- flat surfaces

### Edge Soften Assist

Slightly softens harsh edges in assist modes.

Useful for:

- compositing edges
- overly perfect CG silhouettes
- AI edges with a cutout feeling

### Highlight Rolloff Assist

Compresses bright areas more gently.

Useful for:

- digital highlights
- hard white clipping
- uncinematic specular peaks

### Gamut Guard Assist

Reduces extreme saturation and color outliers.

Useful for:

- LUT problems
- AI color artifacts
- VFX elements that do not sit in the plate

### Sensor Texture Assist

Adds light synthetic sensor texture.

Useful for:

- very clean AI footage
- CGI without camera character
- sterile surfaces

### Temporal Smooth Assist

Very gently mixes neighboring-frame information in assist modes.

Useful for:

- light AI flicker artifacts
- small highlight jumps
- subtle temporal stabilization

Important: keep this control low. Start around `0.10 - 0.25`.

### Texture Seed

Changes the sensor texture pattern.

## Recommended Workflow

### 1. Start With Diagnosis

Set `View Mode` to:

```text
Diagnostic Overlay
```

Check which areas are marked with diagnostic colors.

### 2. Isolate the Cause

Switch to:

```text
Category Colors
```

This helps determine whether the issue is mainly edge-related, highlight-related, texture-related, grade-related, or plasticness-related.

### 3. Check Temporal Behavior

If the shot has movement, also check:

```text
Temporal Stability Matte
```

or:

```text
Temporal Difference Overlay
```

This helps determine whether the fake feeling is coming from missing micro-motion, flicker, or mismatched frame-to-frame stability.

### 4. Check the Matte

Switch to:

```text
Problem Matte
```

If the matte detects too much:

- lower `Sensitivity`
- reduce individual category weights

If the matte detects too little:

- increase `Sensitivity`
- increase the relevant category weights

### 5. Test a Subtle Correction

Switch to:

```text
Reality Match Assist
```

Start with:

```text
Correction Strength: 0.10 - 0.25
```

Only increase it if the correction is clearly helping.

Use `Temporal Smooth Assist` lightly. In most cases, `0.10 - 0.25` is enough.

### 6. Compare the Result

Use Resolve's node bypass or effect bypass to compare before and after.

If the image feels better but not final:

- keep the plugin as a diagnostic reference
- rebuild the final fix in grading or compositing

## Common Use Cases

### AI Footage Looks Plastic

Suggested parameters:

- `Plastic/Smooth Weight`: high
- `Texture Weight`: high
- `Micro Contrast Assist`: slightly increased
- `Sensor Texture Assist`: slightly increased

### CG Element Feels Cut Out

Suggested parameters:

- `Edge/Matte Weight`: high
- `Edge Soften Assist`: gently increased
- use `Diagnostic Overlay` or `Category Colors`

### Highlights Look Digital

Suggested parameters:

- `Highlight/Clip Weight`: high
- `Highlight Rolloff Assist`: increased
- `Gamut Guard Assist`: slightly increased

### Element Does Not Match the Plate Grade

Suggested parameters:

- `Grade/Gamut Weight`: high
- `Gamut Guard Assist`: increased
- finalize the match in Resolve's grade

## What the Plugin Does Not Do

This version is not a fully automated VFX QC system. v2 can read neighboring frames and output temporal diagnostics, but it remains intentionally conservative.

It currently does not:

- analyze a matte against a separate plate input
- reliably identify optical flow or tracking problems
- replace a full VFX QC process
- run real machine learning

Good next steps:

- multi-input plate vs element matching
- matte edge comparison
- shot-level diagnostic report
- native Premiere `.prm` version

## Useful Review Language

Instead of:

```text
It feels fake.
```

The plugin helps you say something more precise:

```text
The highlights are clipping differently from the plate.
```

```text
The edge response is too clean compared to the camera image.
```

```text
The AI element lacks sensor texture in the midtones.
```

```text
The grade is pushing the VFX element out of the plate gamut.
```

That is the real value of the tool: it translates a vague review feeling into a technical direction.

## Troubleshooting

### The Plugin Does Not Appear in Resolve

1. Quit Resolve completely.
2. Check the plugin path:

```text
/Library/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle
```

3. Restart Resolve.
4. Search for `Buckswood` in OpenFX.

### The Plugin Appears, but the Image Does Not Change

Check:

- `View Mode`
- `Sensitivity`
- `Overlay Strength`
- `Correction Strength`

If `Reality Match Assist` is active and `Correction Strength` is set to `0`, the plugin will barely change the image.

### The Overlay Is Too Strong

Reduce:

```text
Overlay Strength
Sensitivity
```

### The Correction Is Too Visible

Reduce:

```text
Correction Strength
Sensor Texture Assist
Micro Contrast Assist
Edge Soften Assist
```

## Distribution

Current installer artifacts:

```text
Buckswood_Fake_Diagnostic_Installer.dmg
Buckswood_Fake_Diagnostic_Installer.pkg
```

Both files are Developer ID signed and notarized.

## Summary

`Buckswood Fake Diagnostic` is a supervision-oriented diagnostic tool. It makes hidden fake-looking signals visible and offers a conservative assist correction.

Its strength is not automatic final image creation. Its strength is helping you find the right cause faster.
