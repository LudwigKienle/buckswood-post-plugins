# IMAX and Dune Lens Research

Scope: public facts only. This file does not copy paid lens-profile datasets,
commercial plugin profiles, private optical measurements, or proprietary
calibration coefficients. The plugin presets are original Buckswood parameter
mappings inspired by publicly documented camera/lens choices.

## Dune: Part One

- Primary public pattern: large-format ARRI Alexa LF capture.
- IMAX 1.43 material: spherical Panavision H Series.
- Scope 2.39 material: Panavision Ultra Vista anamorphic, documented as 1.6x on
  Panavision's product page and as custom-tuned for the production in ASC notes.
- Useful aesthetic translation: soft portrait roll-off, elevated blacks and
  humanity from H Series; deeper anamorphic flare/vertical bias and a more
  organic wide-screen distortion impression from Ultra Vista.

Sources:

- https://theasc.com/article/dune-fear-is-the-mind-killer/
- https://britishcinematographer.co.uk/greig-fraser-asc-acs-dune/
- https://filmmakermagazine.com/112635-we-used-the-entire-sensor-on-the-alexa-lf-every-single-time-dp-greig-fraser-on-shooting-dune-for-imax/
- https://www.panavision.com/camera-and-optics/optics/product-detail/h-h-series
- https://www.panavision.com/camera-and-optics/optics/product-detail/uv-ultra-vista

## Dune: Part Two

- Public pattern: ALEXA 65 as main camera, with ALEXA LF/Mini LF where needed.
- Visual shift: spherical only, composed for IMAX 1.43 while protecting other
  ratios.
- Lens package publicly documented by ASC/ARRI: ARRI Rental Moviecam, Prime DNA,
  IronGlass Soviet-era lenses, Optica Elite, and Zero Optik Rokkor.
- ASC specifically names IronGlass examples ordered for the film: Jupiter-9
  85mm, Helios 44-2 58mm, and Mir-1V 37mm.
- Useful aesthetic translation: Moviecam gets controlled vintage softness with
  higher resolving power; Soviet glass gets stronger edge falloff, swirl,
  warmer character, more coma, and visible texture.

Sources:

- https://theasc.com/article/expanding-view-dune-part-two/
- https://www.arrirental.com/en/about/overview/news/interview-on-dune-part-two-with-greig-fraser-acs-asc
- https://www.arrirental.com/en/lenses/full-frame-lenses/moviecam
- https://www.arrirental.com/en/lenses/full-frame-lenses/moviecam/technical-data

## IMAX 15/70 Reference

- IMAX 15-perf 65/70 is a very large horizontal frame format, with the common
  1.43 presentation ratio.
- Public large-format discussions around Nolan/Hoytema IMAX work repeatedly
  point to 50mm and 80mm as practical face/portrait sweet spots.
- Useful aesthetic translation: cleaner center, low vignette, controlled
  chromatic aberration, high apparent sharpness, and only subtle corner behavior.

Sources:

- https://www.cinematography.net/edited-pages/IMAX15perf_70mmTechSpecs.htm
- https://ymcinema.com/2023/07/25/the-lenses-behind-oppenheimer-modified-panavision-and-an-imax-snorkel-lens/
- https://www.panavision.com/highlights/credits/credits-detail/oppenheimer

## Dune: Part Three Reported Atlas IMAX Lenses

- As of June 30, 2026, public trade coverage reports custom spherical IMAX
  lenses from Atlas Lens Co. for Dune: Part Three.
- Reported focal lengths include 55mm T3.5, 80mm, 105mm, and 150mm, with
  additional focal lengths in development.
- Reported design goals include golden single coating, full 15-perf
  non-vignetting coverage, short close focus, and special corner character.
- Useful aesthetic translation: very low vignette, warmer/golden corner cast,
  controlled bloom, and visible but not heavy flare character.

Sources:

- https://www.newsshooter.com/2026/03/20/atlas-lens-co-spherical-imax-lenses-for-dune-part-three/
- https://www.newsshooter.com/2026/06/05/atlas-lens-co-kaizen-1-5x-anamorphic-lenses-custom-spherical-imax-lenses-at-cine-gear-2026/
- https://www.redsharknews.com/atlas-lens-co-custom-imax-lenses-dune-part-three

## Plugin Translation

These presets are tuned for the existing Buckswood Lens Physics parameter set:

- distortion: radial barrel/pincushion impression
- chromaticAberration/fringing: color split and high-contrast edge behavior
- coma: edge smear and highlight asymmetry
- bloom: highlight bleed and soft halation
- vignette/cornerColor: falloff and edge tint
- swirl: curved edge motion/bokeh impression
- sharpener: perceived center crispness or diffusion

The goal is not a mathematically exact lens clone. It is a stable DaVinci tool
that gives footage the missing large-format glass behavior before or after the
creative grade.
