# BWLOOK v1 Profile Format

`.bwlook` is a compact little-endian binary interchange format. It stores only
aggregate look statistics and never stores reference pixels.

Look DNA V2 remains backward compatible with this BWLOOK1 format. BWLOOK1 files
provide global tone, color, semantic, and texture statistics. V2's 3x3 spatial
analysis is additionally available when a direct JPEG, PNG, or TIFF reference is
loaded. This avoids silently pretending that an older global profile contains
spatial information.

## Header

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | 8 bytes | ASCII magic `BWLOOK1` followed by NUL |
| 8 | uint32 | Format version, currently `1` |
| 12 | uint32 | Float count, currently `35` |
| 16 | uint32 | Number of analyzed image samples |

The header is followed by 35 IEEE-754 float32 values:

- 7 luma quantiles
- luma mean and standard deviation
- average saturation
- local contrast and grain estimates
- chroma mean U/V and 2x2 covariance
- six semantic-zone records, each containing mean U, mean V, and weight

Zone order is skin, sky, foliage, warm light, shadow, highlight.

Readers must reject unknown versions, unexpected float counts, truncated files,
and non-finite values. This makes the format suitable for both the native OFX
reader and local companion tools.
