#include "OpenCLDynamic.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <vector>

namespace buckswood::opencl {
namespace {

constexpr UInt kQueueContext = 0x1090;
constexpr UInt kQueueDevice = 0x1091;
constexpr UInt kProgramBuildLog = 0x1183;

#if defined(_WIN32)
#define BW_CL_CALL __stdcall
#else
#define BW_CL_CALL
#endif

} // namespace

struct Runtime::Functions {
#if defined(_WIN32)
    HMODULE library = nullptr;
#endif
    Int(BW_CL_CALL* getCommandQueueInfo)(CommandQueue, UInt, std::size_t, void*, std::size_t*) = nullptr;
    Program(BW_CL_CALL* createProgramWithSource)(
        Context,
        UInt,
        const char**,
        const std::size_t*,
        Int*) = nullptr;
    Int(BW_CL_CALL* buildProgram)(
        Program,
        UInt,
        const Device*,
        const char*,
        void(BW_CL_CALL*)(Program, void*),
        void*) = nullptr;
    Int(BW_CL_CALL* getProgramBuildInfo)(
        Program,
        Device,
        UInt,
        std::size_t,
        void*,
        std::size_t*) = nullptr;
    Kernel(BW_CL_CALL* createKernel)(Program, const char*, Int*) = nullptr;
    Memory(BW_CL_CALL* createBuffer)(Context, std::uint64_t, std::size_t, void*, Int*) = nullptr;
    Int(BW_CL_CALL* setKernelArg)(Kernel, UInt, std::size_t, const void*) = nullptr;
    Int(BW_CL_CALL* enqueueNDRangeKernel)(
        CommandQueue,
        Kernel,
        UInt,
        const std::size_t*,
        const std::size_t*,
        const std::size_t*,
        UInt,
        const Event*,
        Event*) = nullptr;
    Int(BW_CL_CALL* releaseProgram)(Program) = nullptr;
    Int(BW_CL_CALL* releaseKernel)(Kernel) = nullptr;
    Int(BW_CL_CALL* releaseMemObject)(Memory) = nullptr;
    Int(BW_CL_CALL* releaseEvent)(Event) = nullptr;

    bool complete() const
    {
        return getCommandQueueInfo &&
            createProgramWithSource &&
            buildProgram &&
            getProgramBuildInfo &&
            createKernel &&
            createBuffer &&
            setKernelArg &&
            enqueueNDRangeKernel &&
            releaseProgram &&
            releaseKernel &&
            releaseMemObject &&
            releaseEvent;
    }
};

Runtime& Runtime::instance()
{
    static Runtime runtime;
    return runtime;
}

Runtime::Runtime()
    : functions_(new Functions())
{
#if defined(_WIN32)
    functions_->library = LoadLibraryW(L"OpenCL.dll");
    if (!functions_->library) {
        return;
    }

#define BW_LOAD_CL(name, member) \
    functions_->member = reinterpret_cast<decltype(functions_->member)>( \
        GetProcAddress(functions_->library, name))
    BW_LOAD_CL("clGetCommandQueueInfo", getCommandQueueInfo);
    BW_LOAD_CL("clCreateProgramWithSource", createProgramWithSource);
    BW_LOAD_CL("clBuildProgram", buildProgram);
    BW_LOAD_CL("clGetProgramBuildInfo", getProgramBuildInfo);
    BW_LOAD_CL("clCreateKernel", createKernel);
    BW_LOAD_CL("clCreateBuffer", createBuffer);
    BW_LOAD_CL("clSetKernelArg", setKernelArg);
    BW_LOAD_CL("clEnqueueNDRangeKernel", enqueueNDRangeKernel);
    BW_LOAD_CL("clReleaseProgram", releaseProgram);
    BW_LOAD_CL("clReleaseKernel", releaseKernel);
    BW_LOAD_CL("clReleaseMemObject", releaseMemObject);
    BW_LOAD_CL("clReleaseEvent", releaseEvent);
#undef BW_LOAD_CL
#endif
}

Runtime::~Runtime()
{
#if defined(_WIN32)
    if (functions_->library) {
        FreeLibrary(functions_->library);
    }
#endif
    delete functions_;
}

bool Runtime::available() const
{
    return functions_->complete();
}

bool Runtime::queueContextAndDevice(
    CommandQueue queue,
    Context& context,
    Device& device) const
{
    return available() &&
        functions_->getCommandQueueInfo(
            queue,
            kQueueContext,
            sizeof(context),
            &context,
            nullptr) == kSuccess &&
        functions_->getCommandQueueInfo(
            queue,
            kQueueDevice,
            sizeof(device),
            &device,
            nullptr) == kSuccess;
}

Program Runtime::buildProgram(
    Context context,
    Device device,
    const char* source,
    std::string& buildLog) const
{
    buildLog.clear();
    if (!available()) {
        return nullptr;
    }

    Int error = -1;
    Program program = functions_->createProgramWithSource(
        context,
        1,
        &source,
        nullptr,
        &error);
    if (!program || error != kSuccess) {
        return nullptr;
    }

    error = functions_->buildProgram(
        program,
        1,
        &device,
        "-cl-std=CL1.2",
        nullptr,
        nullptr);
    if (error == kSuccess) {
        return program;
    }

    std::size_t logSize = 0;
    functions_->getProgramBuildInfo(
        program,
        device,
        kProgramBuildLog,
        0,
        nullptr,
        &logSize);
    if (logSize > 0) {
        std::vector<char> log(logSize);
        functions_->getProgramBuildInfo(
            program,
            device,
            kProgramBuildLog,
            log.size(),
            log.data(),
            nullptr);
        buildLog.assign(log.data(), log.data() + log.size());
    }
    functions_->releaseProgram(program);
    return nullptr;
}

Kernel Runtime::createKernel(Program program, const char* name) const
{
    if (!available() || !program) {
        return nullptr;
    }
    Int error = -1;
    Kernel kernel = functions_->createKernel(program, name, &error);
    return error == kSuccess ? kernel : nullptr;
}

Memory Runtime::createBuffer(Context context, std::size_t byteCount) const
{
    if (!available()) {
        return nullptr;
    }
    Int error = -1;
    Memory memory = functions_->createBuffer(
        context,
        kMemReadWrite,
        byteCount,
        nullptr,
        &error);
    return error == kSuccess ? memory : nullptr;
}

bool Runtime::setKernelArg(
    Kernel kernel,
    UInt index,
    std::size_t byteCount,
    const void* value) const
{
    return available() &&
        functions_->setKernelArg(kernel, index, byteCount, value) == kSuccess;
}

bool Runtime::enqueue2D(
    CommandQueue queue,
    Kernel kernel,
    std::size_t width,
    std::size_t height,
    Event waitFor,
    Event* completion) const
{
    const std::size_t global[2] = {width, height};
    return available() &&
        functions_->enqueueNDRangeKernel(
            queue,
            kernel,
            2,
            nullptr,
            global,
            nullptr,
            waitFor ? 1 : 0,
            waitFor ? &waitFor : nullptr,
            completion) == kSuccess;
}

bool Runtime::enqueue1D(
    CommandQueue queue,
    Kernel kernel,
    std::size_t width,
    Event waitFor,
    Event* completion) const
{
    const std::size_t global[1] = {width};
    return available() &&
        functions_->enqueueNDRangeKernel(
            queue,
            kernel,
            1,
            nullptr,
            global,
            nullptr,
            waitFor ? 1 : 0,
            waitFor ? &waitFor : nullptr,
            completion) == kSuccess;
}

void Runtime::releaseProgram(Program program) const
{
    if (available() && program) {
        functions_->releaseProgram(program);
    }
}

void Runtime::releaseKernel(Kernel kernel) const
{
    if (available() && kernel) {
        functions_->releaseKernel(kernel);
    }
}

void Runtime::releaseMemory(Memory memory) const
{
    if (available() && memory) {
        functions_->releaseMemObject(memory);
    }
}

void Runtime::releaseEvent(Event event) const
{
    if (available() && event) {
        functions_->releaseEvent(event);
    }
}

} // namespace buckswood::opencl
