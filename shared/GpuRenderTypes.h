#pragma once

namespace buckswood::gpu {

enum class PixelFormat {
    Float32,
    Byte,
};

struct ImageBuffer {
    void* handle;
    int rowBytes;
    int x1;
    int y1;
    int x2;
    int y2;
    PixelFormat format;
};

struct RenderWindow {
    int x1;
    int y1;
    int x2;
    int y2;
};

} // namespace buckswood::gpu
