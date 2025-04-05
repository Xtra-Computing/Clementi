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

    xrt::kernel gsKernel[KERNEL_NUM];
    xrt::kernel srcReKernel[KERNEL_NUM]; // read engine
    xrt::run gsRun[KERNEL_NUM];
    xrt::run srcReRun[KERNEL_NUM]; // read engine
    xrt::kernel wrPropKernel[KERNEL_NUM];
    xrt::run wrPropRun[KERNEL_NUM];

    xrt::kernel rdPropKernel;
    xrt::run rdPropRun;
    xrt::kernel writeKernel;
    xrt::run writeRun;

    std::vector<std::vector<xrt::bo>> edgeBuffer; // each subpartition owns one buffer
    xrt::bo propBuffer[KERNEL_NUM]; // each subpartition owns one buffer
    xrt::bo propBufferNew[KERNEL_NUM]; // need to divide this buffer to several sub-buffers
    xrt::bo propTemp; // used for store temp apply data for data aggregation

    xrt::kernel applyKernel;
    xrt::run applyRun;
    xrt::bo outDegBuffer;
    xrt::bo outRegBuffer;

    xrt::bo perfCount; // used for perf measurement.
    xrt::bo gsPerf[KERNEL_NUM];

    xrt::device graphDevice;
    xrt::uuid graphUuid;

} graphAccelerator;

#endif /* __MPI_GRAPH_DATA_STRUCTURE_H__ */
