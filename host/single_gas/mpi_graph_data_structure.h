#ifndef __MPI_GRAPH_DATA_STRUCTURE_H__
#define __MPI_GRAPH_DATA_STRUCTURE_H__

#include <cstring>
#include <vector>
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

typedef struct
{

#ifdef NOT_USE_GS
    xrt::kernel readKernel[SUB_PARTITION_NUM/PROCESSOR_NUM]; // replace gsKernel with pure read kernel.
    xrt::run readRun[SUB_PARTITION_NUM/PROCESSOR_NUM];
#else
    xrt::kernel gsKernel[SUB_PARTITION_NUM/PROCESSOR_NUM];
    xrt::kernel srcReKernel[SUB_PARTITION_NUM/PROCESSOR_NUM]; // read engine
    xrt::run gsRun[SUB_PARTITION_NUM/PROCESSOR_NUM];
    xrt::run srcReRun[SUB_PARTITION_NUM/PROCESSOR_NUM]; // read engine
#endif

    xrt::kernel writeKernel[SUB_PARTITION_NUM/PROCESSOR_NUM];
    xrt::run writeRun[SUB_PARTITION_NUM/PROCESSOR_NUM];

    xrt::kernel readVertexKernel[SUB_PARTITION_NUM/PROCESSOR_NUM];
    xrt::run readVertexRun[SUB_PARTITION_NUM/PROCESSOR_NUM];

    xrt::kernel writeVertexKernel[SUB_PARTITION_NUM/PROCESSOR_NUM];
    xrt::run writeVertexRun[SUB_PARTITION_NUM/PROCESSOR_NUM];

    std::vector<std::vector<xrt::bo>> edgeBuffer; // each subpartition owns one buffer
    xrt::bo propBuffer[SUB_PARTITION_NUM/PROCESSOR_NUM]; // each subpartition owns one buffer
    xrt::bo propBufferNew[SUB_PARTITION_NUM/PROCESSOR_NUM]; // need to divide this buffer to several sub-buffers

#if USE_APPLY==true
    xrt::kernel applyKernel;
    xrt::kernel syncKernel;
    xrt::run applyRun;
    xrt::run syncRun;
    xrt::bo outDegBuffer;
    xrt::bo outRegBuffer;
#endif

    xrt::bo perfCount; // used for perf measurement.
    xrt::bo gsPerf[SUB_PARTITION_NUM/PROCESSOR_NUM];

    xrt::device graphDevice;
    xrt::uuid graphUuid;

} graphAccelerator;

#endif /* __MPI_GRAPH_DATA_STRUCTURE_H__ */
