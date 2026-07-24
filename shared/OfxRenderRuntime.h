#pragma once

#include <algorithm>

#include "ofxCore.h"
#include "ofxMultiThread.h"

namespace buckswood::ofx_runtime {

template <typename RowRangeFn>
struct ParallelRowsContext {
    int yBegin;
    int yEnd;
    const RowRangeFn* rowFn;
};

template <typename RowRangeFn>
void parallelRowsCallback(unsigned int threadIndex, unsigned int threadCount, void* customArg)
{
    auto* context = static_cast<ParallelRowsContext<RowRangeFn>*>(customArg);
    const int rowCount = context->yEnd - context->yBegin;
    const int first = context->yBegin +
        (rowCount * static_cast<int>(threadIndex)) / static_cast<int>(threadCount);
    const int last = context->yBegin +
        (rowCount * static_cast<int>(threadIndex + 1)) / static_cast<int>(threadCount);
    if (first < last) {
        (*context->rowFn)(first, last);
    }
}

// Uses the host-owned worker pool so Resolve can coordinate CPU resources
// across concurrent nodes and frames. The callback is synchronous per OFX.
template <typename RowRangeFn>
OfxStatus parallelRows(
    const OfxMultiThreadSuiteV1* threadSuite,
    int yBegin,
    int yEnd,
    const RowRangeFn& rowFn)
{
    const int rowCount = yEnd - yBegin;
    if (rowCount <= 0) {
        return kOfxStatOK;
    }

    unsigned int cpuCount = 1;
    if (!threadSuite ||
        !threadSuite->multiThread ||
        !threadSuite->multiThreadNumCPUs ||
        threadSuite->multiThreadNumCPUs(&cpuCount) != kOfxStatOK) {
        rowFn(yBegin, yEnd);
        return kOfxStatOK;
    }

    const unsigned int threadCount = std::min<unsigned int>(
        std::max(1u, cpuCount),
        static_cast<unsigned int>(rowCount));
    if (threadCount <= 1) {
        rowFn(yBegin, yEnd);
        return kOfxStatOK;
    }

    ParallelRowsContext<RowRangeFn> context{yBegin, yEnd, &rowFn};
    const OfxStatus status = threadSuite->multiThread(
        parallelRowsCallback<RowRangeFn>,
        threadCount,
        &context);
    if (status != kOfxStatOK) {
        rowFn(yBegin, yEnd);
        return kOfxStatOK;
    }
    return status;
}

} // namespace buckswood::ofx_runtime
