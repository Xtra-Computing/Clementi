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
#include "experimental/xrt_queue.h"

// network configuration : each networklayer has an IP address.
std::map<int, std::map<std::string, std::string>> FPGA_config = \
   {{0 , {{"ip_addr" , "192.168.0.201"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "201"}, {"MAC_addr" , "00:0a:35:02:9d:c8"}}}, \
    {1 , {{"ip_addr" , "192.168.0.202"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "202"}, {"MAC_addr" , "00:0a:35:02:9d:c9"}}}, \
    {2 , {{"ip_addr" , "192.168.0.203"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "203"}, {"MAC_addr" , "00:0a:35:02:9d:ca"}}}, \
    {3 , {{"ip_addr" , "192.168.0.204"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "204"}, {"MAC_addr" , "00:0a:35:02:9d:cb"}}}, \
    {4 , {{"ip_addr" , "192.168.0.205"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "205"}, {"MAC_addr" , "00:0a:35:02:9d:cc"}}}, \
    {5 , {{"ip_addr" , "192.168.0.206"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "206"}, {"MAC_addr" , "00:0a:35:02:9d:cd"}}}, \
    {6 , {{"ip_addr" , "192.168.0.207"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "207"}, {"MAC_addr" , "00:0a:35:02:9d:ce"}}}, \
    {7 , {{"ip_addr" , "192.168.0.208"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "208"}, {"MAC_addr" , "00:0a:35:02:9d:cf"}}}};


int acceleratorInit(graphInfo *info, graphAccelerator *acc)
{
    // load xclbin file
    int world_size = -1;
    int world_rank = -1;
    std::string xclbin_file;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    if (world_size < 0) {
        log_error("world size error! ");
        return 0;
    } else if (world_size == 1) {
#ifdef NOT_USE_GS
        xclbin_file = "./build_dir_hw_thunder_gp_pure_apply_/kernel.link.xclbin";
#else
        xclbin_file = "./build_dir_hw_thunder_gp_wcc/kernel.link.xclbin";
#endif
    } else { // multi node
#ifdef NOT_USE_GS
        if (world_rank == 0) {
            xclbin_file = "./build_dir_hw_netgp_pure_net_gas/kernel.link.xclbin"; // GAS worker
        } else {
            xclbin_file = "./build_dir_hw_netgp_pure_net_gs/kernel.link.xclbin"; // GS worker
        }
#else
        if (world_rank == 0) {
            xclbin_file = "./build_dir_hw_netgp_gas/kernel.link.xclbin"; // GAS worker
        } else {
            xclbin_file = "./build_dir_hw_netgp_gs/kernel.link.xclbin"; // GS worker
        }
#endif
    }

    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(xclbin_file);

    log_debug("[INFO] Processor %d : Xclbin load done ", world_rank);

    //check linkstatus. Only for multi node case. Single node skip this.
#if (PROCESSOR_NUM > 1)

    bool linkStatus = false;
    AlveoVnxCmac cmac_0 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 0);
    AlveoVnxCmac cmac_1 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 1);
    linkStatus = (cmac_0.readRegister("stat_rx_status") & 0x1) && (cmac_1.readRegister("stat_rx_status") & 0x1);
    while(!linkStatus) {
        sleep(1);
        linkStatus = (cmac_0.readRegister("stat_rx_status") & 0x1) && (cmac_1.readRegister("stat_rx_status") & 0x1);
        log_debug("[INFO] Processor %d, check linkstatus ...", world_rank);
    }
    log_debug("[INFO] Processor %d, check linkstatus ...  done", world_rank);


    // set socket table, run arp discovery;
    // ring topology; 
    // upper worker's networklayer 0 <==> current worker's networklayer 1
    // lower worker's networklayer 1 <==> current worker's networklayer 0
    int upper_worker_idx = (world_rank == 0)? (world_size - 1) : (world_rank - 1);
    int lower_worker_idx = (world_rank == (world_size - 1))? 0 : (world_rank + 1);

    AlveoVnxNetworkLayer netlayer_0 = AlveoVnxNetworkLayer(acc->graphDevice, acc->graphUuid, 0);
    AlveoVnxNetworkLayer netlayer_1 = AlveoVnxNetworkLayer(acc->graphDevice, acc->graphUuid, 1);

    netlayer_0.setAddress(FPGA_config[world_rank*2+1]["ip_addr"], FPGA_config[world_rank*2+1]["MAC_addr"]);
    netlayer_0.setSocket(FPGA_config[lower_worker_idx*2]["ip_addr"], stoi(FPGA_config[lower_worker_idx*2]["tx_port"]), 5001, 0); // set recv socket
    netlayer_0.setSocket(FPGA_config[lower_worker_idx*2]["ip_addr"], 5001, stoi(FPGA_config[world_rank*2+1]["tx_port"]), 1); // set send socket
    netlayer_0.getSocketTable();

    netlayer_1.setAddress(FPGA_config[world_rank*2]["ip_addr"], FPGA_config[world_rank*2]["MAC_addr"]);
    netlayer_1.setSocket(FPGA_config[upper_worker_idx*2+1]["ip_addr"], stoi(FPGA_config[upper_worker_idx*2+1]["tx_port"]), 5001, 0); // set recv socket
    netlayer_1.setSocket(FPGA_config[upper_worker_idx*2+1]["ip_addr"], 5001, stoi(FPGA_config[world_rank*2]["tx_port"]), 1); // set send socket
    netlayer_1.getSocketTable();

    bool ARP_ready = false;
    while(!ARP_ready) {
        netlayer_0.runARPDiscovery();
        netlayer_1.runARPDiscovery();
        usleep(500000);
        ARP_ready = (netlayer_0.IsARPTableFound(FPGA_config[lower_worker_idx*2]["ip_addr"])) \
                    && (netlayer_1.IsARPTableFound(FPGA_config[upper_worker_idx*2+1]["ip_addr"]));
        log_debug("[INFO] Processor %d, wait ARP ... ", world_rank);
    }
    log_debug("[INFO] Processor %d, wait ARP ... done ", world_rank);

#endif

    log_trace("network connect, world rank %d", world_rank);


    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;
    std::string id, krnl_name;
    for (int i = 0; i < (SUB_PARTITION_NUM / PROCESSOR_NUM); i++)
    {
        id = std::to_string(i + 1);
#ifdef NOT_USE_GS
        krnl_name = "streamRead:{streamRead_" + id + "}";
        acc->readKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
#else
        krnl_name = "gs:{gs_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "src_cache_read_engine:{src_cache_read_engine_" + id + "}";
        acc->srcReKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
#endif

        krnl_name = "streamWrite:{streamWrite_" + id + "}";
        acc->writeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

    if (world_rank == 0) { // GAS worker, additional apply kernel and sync kernel
        krnl_name = "streamApply:{streamApply_1}";
        acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "streamSync:{streamSync_1}";
        acc->syncKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

    log_trace("kernel init done, world rank %d", world_rank);


    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;

#ifndef NOT_USE_GS 
    acc->edgeBuffer.resize(info->partitionNum);
    for (int p = 0; p < info->partitionNum; p++) {
        acc->edgeBuffer[p].resize(kernel_per_node);
        for (int sp = 0; sp < kernel_per_node; sp++) {
            edge_size_bytes = info->chunkProp[p][sp].edgeNumChunk * 2 * sizeof(int); // each edge has two vertex, vertex index uses int type;
            acc->edgeBuffer[p][sp] = xrt::bo(acc->graphDevice, edge_size_bytes, acc->gsKernel[sp].group_id(0));
            info->chunkEdgeData[p][sp] = acc->edgeBuffer[p][sp].map<int*>();
        }
    }
#endif

    for (int sp = 0; sp < kernel_per_node; sp++) {
#ifdef NOT_USE_GS
        prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex prop;
        acc->propBuffer[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->readKernel[sp].group_id(0));
        info->chunkPropData[sp] = acc->propBuffer[sp].map<int*>();
#else
        prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex prop;
        acc->propBuffer[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->srcReKernel[sp].group_id(0));
        info->chunkPropData[sp] = acc->propBuffer[sp].map<int*>();

        prop_size_bytes = 4;
        acc->gsPerf[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->writeKernel[sp].group_id(5));
        info->gsPerfCount[sp] = acc->gsPerf[sp].map<int*>();
#endif
        prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // new vertex prop;
        acc->propBufferNew[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->writeKernel[sp].group_id(0));
        info->chunkPropDataNew[sp] = acc->propBufferNew[sp].map<int*>();
    }

    if (world_rank == 0) { // GAS worker
#if USE_APPLY==true
        outDeg_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex number * sizeof(int)
        acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(3));
        acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(4));
        info->chunkOutDegData = acc->outDegBuffer.map<int*>();
        info->chunkOutRegData = acc->outRegBuffer.map<int*>();

        // prop_size_bytes = 4; // long int for perf count.
        // acc->perfCount = xrt::bo(acc->graphDevice, prop_size_bytes, acc->syncKernel.group_id(5));
        // info->perfCount = acc->perfCount.map<int*>();
#endif
    }

    log_trace("kernel map done, world rank %d", world_rank);
    return 0;
}


void setAccKernelArgs(graphInfo *info, graphAccelerator *acc)
{
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    if (world_rank < 0) log_error("world rank error !");
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    for (int sp = 0; sp < kernel_per_node; sp++) {
#ifdef NOT_USE_GS
        acc->readRun[sp] = xrt::run(acc->readKernel[sp]);
#else
        acc->gsRun[sp] = xrt::run(acc->gsKernel[sp]);
        acc->gsRun[sp].set_arg(3, 0);
        acc->gsRun[sp].set_arg(4, PARTITION_SIZE); // should be dest vertex number, but I align, need modify
        acc->srcReRun[sp] = xrt::run(acc->srcReKernel[sp]);
#endif
        acc->writeRun[sp] = xrt::run(acc->writeKernel[sp]);
    }

    if (world_rank == 0) { // GAS worker
#if USE_APPLY==true
        acc->applyRun = xrt::run(acc->applyKernel);
        acc->applyRun.set_arg(3, acc->outDegBuffer);
        acc->applyRun.set_arg(4, acc->outRegBuffer);

        acc->syncRun = xrt::run(acc->syncKernel);
        acc->syncRun.set_arg(2, 1);
        if (world_size == 1) {
            acc->syncRun.set_arg(3, 1024*1024); // FIFO length, 66536
        } else{
            acc->syncRun.set_arg(3, 2048); // FIFO length, 2048
        }
        acc->syncRun.set_arg(4, PARTITION_SIZE); // vertex number
        // acc->syncRun.set_arg(5, acc->perfCount);
#endif
    }
}

void partitionTransfer(graphInfo *info, graphAccelerator *acc)
{
    // Synchronize buffer content with device side
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");

    if (world_rank == 0) { // GAS worker
#if USE_APPLY==true
        acc->outDegBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        // info->perfCount[0] = 0;
        // acc->perfCount.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#endif
    }

    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
#ifndef NOT_USE_GS
        for (int p = 0; p < info->partitionNum; p++) {
            acc->edgeBuffer[p][sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
#endif
    }
}


int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator *acc) {
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    log_debug("[INFO] SuperStep %d Execute at processor %d ", super_step , world_rank);
    int pingpong = super_step % 2;

    // xrt::queue main_queue;

    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
#ifdef NOT_USE_GS
            auto readBuf = (pingpong == 0)? acc->propBuffer[sp]: acc->propBufferNew[sp];
            acc->readRun[sp].set_arg(0, readBuf);
            acc->readRun[sp].set_arg(2, (p * PARTITION_SIZE)); // vertex offset
            acc->readRun[sp].set_arg(3, PARTITION_SIZE); // vertex number
            acc->readRun[sp].start();
#endif
            auto writeBuf = (pingpong == 0)? acc->propBufferNew[sp]: acc->propBuffer[sp];
            // main_queue.enqueue([&] {
            //     auto writeBuf = (pingpong == 0)? acc->propBufferNew[sp]: acc->propBuffer[sp]; 
            //     acc->writeRun[sp].set_arg(0, writeBuf);
            //     });
            // main_queue.enqueue([&] {acc->writeRun[sp].set_arg(2, (p * PARTITION_SIZE));});
            // main_queue.enqueue([&] {acc->writeRun[sp].set_arg(3, PARTITION_SIZE);});
            acc->writeRun[sp].set_arg(0, writeBuf);
            acc->writeRun[sp].set_arg(2, (p * PARTITION_SIZE)); // vertex offset
            acc->writeRun[sp].set_arg(3, PARTITION_SIZE); // vertex number
#ifndef NOT_USE_GS
            // main_queue.enqueue([&] {acc->writeRun[sp].set_arg(5, acc->gsPerf[sp]);});
            acc->writeRun[sp].set_arg(5, acc->gsPerf[sp]);

            auto reBuf = (pingpong == 0)? acc->propBuffer[sp]: acc->propBufferNew[sp];
            acc->srcReRun[sp].set_arg(0, reBuf);
            acc->srcReRun[sp].start();
            acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
            acc->gsRun[sp].set_arg(2, info->chunkProp[p][sp].edgeNumChunk * 2);
            acc->gsRun[sp].start();
            // main_queue.enqueue([&] {
            //     auto reBuf = (pingpong == 0)? acc->propBuffer[sp]: acc->propBufferNew[sp];
            //     acc->srcReRun[sp].set_arg(0, reBuf);
            //     });
            // main_queue.enqueue([&] {acc->srcReRun[sp].start();});
            // main_queue.enqueue([&] {acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);});
            // main_queue.enqueue([&] {acc->gsRun[sp].set_arg(2, info->chunkProp[p][sp].edgeNumChunk * 2);});
            // main_queue.enqueue([&] {acc->gsRun[sp].start();});

#endif
            // main_queue.enqueue([&] {acc->writeRun[sp].start();});
            acc->writeRun[sp].start();
        }

        if (world_rank == 0) {
#if USE_APPLY==true
            auto applyBuf = (pingpong == 0)? acc->propBuffer[0]: acc->propBufferNew[0];
            // main_queue.enqueue([&] {
            //     auto applyBuf = (pingpong == 0)? acc->propBuffer[0]: acc->propBufferNew[0];
            //     acc->applyRun.set_arg(0, applyBuf);
            //     });
            // main_queue.enqueue([&] {acc->applyRun.set_arg(5, (p * PARTITION_SIZE));});
            // main_queue.enqueue([&] {acc->applyRun.set_arg(6, PARTITION_SIZE);});
            // main_queue.enqueue([&] {acc->applyRun.set_arg(7, 0);});
            // main_queue.enqueue([&] {acc->applyRun.start();});
            // main_queue.enqueue([&] {acc->syncRun.start();});
            acc->applyRun.set_arg(0, applyBuf); // apply in SLR0
            acc->applyRun.set_arg(5, (p * PARTITION_SIZE)); // vertex offset
            acc->applyRun.set_arg(6, PARTITION_SIZE); // vertex number in each partition
            acc->applyRun.set_arg(7, 0);
            acc->applyRun.start();
            // usleep(100000); // For pure_net count test, wait for other kernel start done. set as 100ms. important !!
            acc->syncRun.start();
#endif
        }


        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
#ifdef NOT_USE_GS
            acc->readRun[sp].wait();
#else
            // main_queue.enqueue([&] {acc->gsRun[sp].wait();});
            // main_queue.enqueue([&] {acc->srcReRun[sp].wait();});
            acc->gsRun[sp].wait();
            acc->srcReRun[sp].wait();
#endif
            // main_queue.enqueue([&] {acc->writeRun[sp].wait();});
            acc->writeRun[sp].wait();
        }

        if (world_rank == 0) {
#if USE_APPLY==true
            // main_queue.enqueue([&] {acc->applyRun.wait();});
            // main_queue.enqueue([&] {acc->syncRun.wait();});
            acc->applyRun.wait();
            acc->syncRun.wait();

#if NOT_USE_GS
            // acc->perfCount.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            // int count_num = info->perfCount[0];
            // int time_us = count_num * 4 / 1000; // node work at 250 MHz;
            // std::cout << "(" << p << ")  PerfCount = " << count_num << " " << time_us << " us"<< std::endl;
            // info->perfCount[0] = 0;
            // acc->perfCount.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#endif
#endif
        }

#if 0
        // for perf evaluation
        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->gsPerf[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            std::cout << "[" << p << "][" << world_rank*PROCESSOR_NUM + sp << "] GS perf = " \
                    << info->gsPerfCount[sp][0] << ", time = " \
                    << info->gsPerfCount[sp][0]/250 << " us" << std::endl;
        }
#endif
    }

    return 0;
}


int resultTransfer(graphInfo *info, graphAccelerator * acc, int run_counter)
{
    // Transfer device buffer content to host side
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == 0) {
#if USE_APPLY==true
        acc->outRegBuffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
#endif
    }

    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    if (world_rank == 0) { // for result verify
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
    }

    log_trace("[INFO] Sync data (temp, outReg) to host ");

    return 0;
}

