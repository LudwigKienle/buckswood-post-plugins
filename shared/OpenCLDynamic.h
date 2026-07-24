#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace buckswood::opencl {

using Int = std::int32_t;
using UInt = std::uint32_t;
using CommandQueue = void*;
using Context = void*;
using Device = void*;
using Program = void*;
using Kernel = void*;
using Memory = void*;
using Event = void*;

constexpr Int kSuccess = 0;
constexpr std::uint64_t kMemReadWrite = 1u << 0;

class Runtime {
public:
    static Runtime& instance();

    bool available() const;
    bool queueContextAndDevice(CommandQueue queue, Context& context, Device& device) const;
    Program buildProgram(
        Context context,
        Device device,
        const char* source,
        std::string& buildLog) const;
    Kernel createKernel(Program program, const char* name) const;
    Memory createBuffer(Context context, std::size_t byteCount) const;
    bool setKernelArg(Kernel kernel, UInt index, std::size_t byteCount, const void* value) const;
    bool enqueue2D(
        CommandQueue queue,
        Kernel kernel,
        std::size_t width,
        std::size_t height,
        Event waitFor,
        Event* completion) const;
    bool enqueue1D(
        CommandQueue queue,
        Kernel kernel,
        std::size_t width,
        Event waitFor,
        Event* completion) const;
    void releaseProgram(Program program) const;
    void releaseKernel(Kernel kernel) const;
    void releaseMemory(Memory memory) const;
    void releaseEvent(Event event) const;

private:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    struct Functions;
    Functions* functions_;
};

} // namespace buckswood::opencl
