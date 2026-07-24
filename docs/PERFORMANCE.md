# GPU and CPU Performance

## What changed

Lens Physics v0.5 and AI Photorealizer v0.3 now use the GPU buffers supplied
by the OpenFX host:

- Apple Silicon and Intel Mac: Metal
- Windows NVIDIA and AMD: OpenCL buffer rendering
- Unsupported hosts, byte images, or unavailable GPU contexts: CPU fallback

The other Resolve OFX plugins use Resolve's `OfxMultiThreadSuiteV1` worker
pool. This avoids creating a new set of operating-system threads for every
node and frame, and lets Resolve coordinate concurrent clips and nodes.

## Quality contract

The performance paths do not use FP16, reduced sample counts, lower-resolution
proxies, or simplified effects. Lens Physics keeps the complete bilinear
sampling, chromatic aberration, fringing, bloom, coma, vignette, sharpener,
Overdrive Edge Guard, and Adjustment Layer Guard pipeline. Both GPU paths use
Float32.

Metal is compiled in safe math mode. Automated tests compare GPU output with
the CPU reference:

| Plugin | Maximum measured CPU/Metal difference |
| --- | ---: |
| AI Photorealizer | `0.0000000` |
| Lens Physics | `0.00000394` |

The test threshold is `0.000005` for Photorealizer and `0.000020` for Lens
Physics. CPU smoke-test reference values remain unchanged.

## Measured result

Benchmark machine: Apple M3 Ultra, 96-core GPU, 128 GB unified memory.
Input: 1920 x 1080 RGBA Float32. Three warm-cache runs produced:

| Plugin | Observed CPU | Observed Metal | Speedup range |
| --- | ---: | ---: | ---: |
| AI Photorealizer | 6.28-8.88 ms | 2.16-3.06 ms | 2.25x-4.10x |
| Lens Physics | 44.41-53.39 ms | 8.21-11.06 ms | 4.06x-6.50x |

Run the benchmark locally:

```bash
bash scripts/run_gpu_performance_benchmark.sh
```

Different GPUs, drivers, frame sizes, node graphs, and Resolve render settings
will produce different timings.

## Caching

The Metal pipeline is compiled once per Metal device and reused. The OpenCL
program and kernels are compiled once per OpenCL context and reused. Lens
models and frame-constant parameters are prepared once per render instead of
once per pixel.

The plugins deliberately do not keep a second full-frame image cache. Resolve
already owns render caching, and an internal frame cache would duplicate large
Float32 buffers, complicate invalidation for keyframes and upstream node
changes, and risk stale frames.

## Platform validation

- macOS Metal: compiled, executed, and compared pixel-by-pixel on Apple Silicon
- Windows OpenCL: kernels pass OpenCL 1.2 syntax validation and both OFX
  plug-ins cross-compile into the Windows installer
- Windows GPU runtime: requires final playback validation on representative
  NVIDIA and AMD systems because those drivers are not available on the macOS
  build machine

The CPU fallback is always retained.
