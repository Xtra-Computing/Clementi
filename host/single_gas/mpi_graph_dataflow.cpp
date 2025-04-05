#include <string>
#include <iostream>
#include <map>
#include <chrono>
#include <unistd.h>
#include <fstream>

// #include "host_graph_sw.h"
// #include "host_graph_scheduler.h"
#include "mpi_graph_api.h"
#include "mpi_host.h"
#include "../network/network.h"


// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

// network configuration : 0 for GAS worker, other GA worker.
std::map<int, std::map<std::string, std::string>> FPGA_config = \
   {{0 , {{"ip_addr" , "192.168.0.201"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "201"}, {"MAC_addr" , "00:0a:35:02:9d:c9"}}}, \
    {1 , {{"ip_addr" , "192.168.0.202"}, {"tx_port" , "62177"}, {"rx_port" , "5001"}, {"idx" , "202"}, {"MAC_addr" , "00:0a:35:02:9d:ca"}}}, \
    {2 , {{"ip_addr" , "192.168.0.203"}, {"tx_port" , "61452"}, {"rx_port" , "5001"}, {"idx" , "203"}, {"MAC_addr" , "00:0a:35:02:9d:cb"}}}, \
    {3 , {{"ip_addr" , "192.168.0.204"}, {"tx_port" , "60523"}, {"rx_port" , "5001"}, {"idx" , "204"}, {"MAC_addr" , "00:0a:35:02:9d:cc"}}}};


int acceleratorInit(graphInfo *info, graphAccelerator *acc)
{
    // load xclbin file
    auto xclbin_file = "./build_dir_hw_single_gas_pr/kernel.link.xclbin";
    log_info("[INFO] Load xclbin file : %s", xclbin_file);
    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(xclbin_file);

    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;
    std::string id, krnl_name;
    for (int i = 0; i < (SUB_PARTITION_NUM / PROCESSOR_NUM); i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "gs:{gs_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "src_cache_read_engine:{src_cache_read_engine_" + id + "}";
        acc->srcReKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "streamWrite:{streamWrite_" + id + "}";
        acc->writeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

    krnl_name = "streamApply:{streamApply_1}";
    acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    krnl_name = "streamSync:{streamSync_1}";
    acc->syncKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());


    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;

    acc->edgeBuffer.resize(info->partitionNum);
    for (int p = 0; p < info->partitionNum; p++) {
        acc->edgeBuffer[p].resize(kernel_per_node);
        for (int sp = 0; sp < kernel_per_node; sp++) {
            edge_size_bytes = info->chunkProp[p][sp].edgeNumChunk * 2 * sizeof(int); // each edge has two vertex, vertex index uses int type;
            acc->edgeBuffer[p][sp] = xrt::bo(acc->graphDevice, edge_size_bytes, acc->gsKernel[sp].group_id(0));
            info->chunkEdgeData[p][sp] = acc->edgeBuffer[p][sp].map<int*>();
        }
    }

    for (int sp = 0; sp < kernel_per_node; sp++) {
        prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex prop;
        acc->propBuffer[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->srcReKernel[sp].group_id(0));
        info->chunkPropData[sp] = acc->propBuffer[sp].map<int*>();

        prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // new vertex prop;
        acc->propBufferNew[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->writeKernel[sp].group_id(0));
        info->chunkPropDataNew[sp] = acc->propBufferNew[sp].map<int*>();
    }

#if USE_APPLY==true
    outDeg_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex number * sizeof(int)
    acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(3));
    acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(4));
    info->chunkOutDegData = acc->outDegBuffer.map<int*>();
    info->chunkOutRegData = acc->outRegBuffer.map<int*>();
#endif


    return 0;
}


void setAccKernelArgs(graphInfo *info, graphAccelerator *acc)
{
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    for (int sp = 0; sp < kernel_per_node; sp++) {
        acc->gsRun[sp] = xrt::run(acc->gsKernel[sp]);
        acc->gsRun[sp].set_arg(3, 0);
        acc->gsRun[sp].set_arg(4, PARTITION_SIZE); // should be dest vertex number, but I align, need modify
        acc->srcReRun[sp] = xrt::run(acc->srcReKernel[sp]);
        acc->writeRun[sp] = xrt::run(acc->writeKernel[sp]);
    }

#if USE_APPLY==true
    acc->applyRun = xrt::run(acc->applyKernel);
    acc->applyRun.set_arg(3, acc->outDegBuffer);
    acc->applyRun.set_arg(4, acc->outRegBuffer);

    acc->syncRun = xrt::run(acc->syncKernel);
    acc->syncRun.set_arg(2, 1);
    acc->syncRun.set_arg(3, 2048); // FIFO length, 2048
    acc->syncRun.set_arg(4, PARTITION_SIZE); // vertex number

#endif

}

void partitionTransfer(graphInfo *info, graphAccelerator *acc)
{
    // Synchronize buffer content with device side

#if USE_APPLY==true
    acc->outDegBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#endif

    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    for (int sp = 0; sp < (kernel_per_node); sp++) {
        acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        for (int p = 0; p < info->partitionNum; p++) {
            acc->edgeBuffer[p][sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }
}


int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator *acc) {

    int pingpong = super_step % 2;
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    int elapsed_p = 0;

    for (int p = 0; p < info->partitionNum; p++) {

        for (int sp = 0; sp < (kernel_per_node); sp++) {
            auto reBuf = (pingpong == 0)? acc->propBuffer[sp]: acc->propBufferNew[sp];
            acc->srcReRun[sp].set_arg(0, reBuf);
            acc->srcReRun[sp].start();
            acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
            acc->gsRun[sp].set_arg(2, info->chunkProp[p][sp].edgeNumChunk * 2);
            auto writeBuf = (pingpong == 0)? acc->propBufferNew[sp]: acc->propBuffer[sp];
            acc->writeRun[sp].set_arg(0, writeBuf);
            acc->writeRun[sp].set_arg(2, (p * PARTITION_SIZE)); // vertex offset
            acc->writeRun[sp].set_arg(3, PARTITION_SIZE); // vertex number
            acc->writeRun[sp].start();
            acc->gsRun[sp].start();
        }

        auto applyBuf = (pingpong == 0)? acc->propBuffer[0]: acc->propBufferNew[0];
        acc->applyRun.set_arg(0, applyBuf); // apply in SLR0
        acc->applyRun.set_arg(5, (p * PARTITION_SIZE)); // vertex offset
        acc->applyRun.set_arg(6, PARTITION_SIZE); // vertex number in each partition
        acc->applyRun.set_arg(7, 0);
        acc->applyRun.start();
        acc->syncRun.start();

        for (int sp = 0; sp < (kernel_per_node); sp++) {
            acc->gsRun[sp].wait();
            acc->srcReRun[sp].wait();
            acc->writeRun[sp].wait();
        }
        acc->applyRun.wait();
        acc->syncRun.wait();

    }

    return 0;
}


int resultTransfer(graphInfo *info, graphAccelerator * acc, int run_counter)
{
    // Transfer device buffer content to host side
#if USE_APPLY==true
    acc->outRegBuffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
#endif

    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    for (int sp = 0; sp < (kernel_per_node); sp++) {
        acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

// for cmodel verification.
/*
    std::ofstream outputFile("./run_verify/hw_data.bin", std::ios::binary);
    if (outputFile.is_open()) {
        if (run_counter % 2 == 0) {
            outputFile.write(reinterpret_cast<char*>(&(info->compressedVertexNum)), sizeof(int));
            outputFile.write(reinterpret_cast<char*>(info->chunkPropData[1]), (sizeof(int) * (info->compressedVertexNum)));
        } else {
            outputFile.write(reinterpret_cast<char*>(&(info->compressedVertexNum)), sizeof(int));
            outputFile.write(reinterpret_cast<char*>(info->chunkPropDataNew[1]), (sizeof(int) * (info->compressedVertexNum)));
        }
        outputFile.close();
    }
*/

    return 0;
}

