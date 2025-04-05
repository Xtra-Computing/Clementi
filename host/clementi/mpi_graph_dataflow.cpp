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
    if (world_size <= 1) {
        log_error("world size error! ");
        return 0;
    } else { // multi node
        xclbin_file = "./build_dir_hw_clementi_pr/kernel.link.xclbin";
    }

    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(xclbin_file);
    log_debug("[INFO] Processor %d : Xclbin load done ", world_rank);

    // check linkstatus. Only for multi node case. Single node skip this.
#if (PROCESSOR_NUM > 1)

    bool linkStatus = false;
    AlveoVnxCmac cmac_0 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 0);
    linkStatus = cmac_0.readRegister("stat_rx_status") & 0x1;
    while(!linkStatus) {
        sleep(1);
        linkStatus = cmac_0.readRegister("stat_rx_status") & 0x1;
        log_debug("[INFO] Processor %d, check linkstatus ...", world_rank);
    }
    // std::cout << "[INFO] Processor " << world_rank << ", check linkstatus ...  done" << std::endl;

    // set socket table, run arp discovery;
    // upper worker => lower worker: 3->0, 0->1, 1->2, 2->3;
    int upper_worker_idx = (world_rank == 0)? (world_size - 1) : (world_rank - 1);
    int lower_worker_idx = (world_rank == (world_size - 1))? 0 : (world_rank + 1);

    AlveoVnxNetworkLayer netlayer_0 = AlveoVnxNetworkLayer(acc->graphDevice, acc->graphUuid, 0);
    netlayer_0.setAddress(FPGA_config[world_rank]["ip_addr"], FPGA_config[world_rank]["MAC_addr"]);
    netlayer_0.setSocket(FPGA_config[upper_worker_idx]["ip_addr"], stoi(FPGA_config[upper_worker_idx]["tx_port"]), 5001, 0); // set recv socket
    netlayer_0.setSocket(FPGA_config[lower_worker_idx]["ip_addr"], 5001, stoi(FPGA_config[world_rank]["tx_port"]), 1); // set send socket
    netlayer_0.getSocketTable();

    bool ARP_ready = false;
    log_debug("[INFO] Processor %d, wait ARP ... ", world_rank);
    while(!ARP_ready) {
        netlayer_0.runARPDiscovery();
        usleep(500000);
        ARP_ready = netlayer_0.IsARPTableFound(FPGA_config[lower_worker_idx]["ip_addr"]);
    }
    log_debug("[INFO] Processor %d, wait ARP ... done", world_rank); 

#endif

    log_trace("network connect, world rank %d", world_rank);

    int kernel_per_node = KERNEL_NUM;
    std::string id, krnl_name;
    for (int i = 0; i < (KERNEL_NUM); i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "gs:{gs_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "src_cache_read_engine:{src_cache_read_engine_" + id + "}";
        acc->srcReKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "writeProp:{writeProp_" + id + "}";
        acc->wrPropKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

    krnl_name = "readProp:{readProp_1}";
    acc->rdPropKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    krnl_name = "streamWrite:{streamWrite_1}";
    acc->writeKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    krnl_name = "streamApply:{streamApply_1}";
    acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());

    log_trace("kernel init done, world rank %d", world_rank);

    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;

    int schedule_num = info->scheduleItem.size();
    acc->edgeBuffer.resize(schedule_num);
    for (int p = 0; p < schedule_num; p++) {
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
        acc->propBufferNew[sp] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->wrPropKernel[sp].group_id(0));
        info->chunkPropDataNew[sp] = acc->propBufferNew[sp].map<int*>();
    }

    prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex prop;
    acc->propTemp = xrt::bo(acc->graphDevice, prop_size_bytes, acc->rdPropKernel.group_id(0));
    info->propTemp = acc->propTemp.map<int*>();

    outDeg_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex number * sizeof(int)
    acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(3));
    acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(4));
    info->chunkOutDegData = acc->outDegBuffer.map<int*>();
    info->chunkOutRegData = acc->outRegBuffer.map<int*>();

    log_trace("kernel map done, world rank %d", world_rank);
    return 0;
}


void partitionTransfer(graphInfo *info, graphAccelerator *acc)
{
    // Synchronize buffer content with device side
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");

    acc->outDegBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    acc->propTemp.sync(XCL_BO_SYNC_BO_TO_DEVICE); // for temp prop store

    for (int sp = 0; sp < KERNEL_NUM; sp++) {
        acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);

        int schedule_num = info->scheduleItem.size();
        for (int p = 0; p < schedule_num; p++) {
            acc->edgeBuffer[p][sp].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }

    }
}


void setAccKernelArgs(graphInfo *info, graphAccelerator *acc)
{
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    if (world_rank < 0) log_error("world rank error !");
    int kernel_per_node = KERNEL_NUM;

    for (int sp = 0; sp < kernel_per_node; sp++) {
        acc->gsRun[sp] = xrt::run(acc->gsKernel[sp]);
        acc->gsRun[sp].set_arg(3, 0);
        acc->gsRun[sp].set_arg(4, PARTITION_SIZE); // should be dest vertex number, but I align, need modify
        acc->srcReRun[sp] = xrt::run(acc->srcReKernel[sp]);
        acc->wrPropRun[sp] = xrt::run(acc->wrPropKernel[sp]);
    }

    acc->applyRun = xrt::run(acc->applyKernel);
    acc->applyRun.set_arg(3, acc->outDegBuffer);
    acc->applyRun.set_arg(4, acc->outRegBuffer);

    acc->rdPropRun = xrt::run(acc->rdPropKernel);
    acc->writeRun = xrt::run(acc->writeKernel);
}


int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator *acc) {
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    log_debug("[INFO] SuperStep %d Execute at processor %d ", super_step , world_rank);
    int pingpong = super_step % 2;
    int total_p_num = (info->alignedCompressedVertexNum / PARTITION_SIZE);
    int packet_num = total_p_num * (((PARTITION_SIZE >> 4) + 20) / 21);

    int schedule_num = info->scheduleItem.size();

    for (int p = 0; p < (schedule_num + 1); p++) {
        // auto start_p = std::chrono::steady_clock::now();
        if (p == 0) {
            // first partition
            // std::cout << world_rank << "] => first partition " << std::endl;
            for (int sp = 0; sp < KERNEL_NUM; sp++) {
                auto writeBuf = (pingpong == 0)? acc->propBufferNew[sp]: acc->propBuffer[sp];
                acc->wrPropRun[sp].set_arg(0, writeBuf);
                acc->wrPropRun[sp].set_arg(2, packet_num); // total packet number
                acc->wrPropRun[sp].start();

                auto reBuf = (pingpong == 0)? acc->propBuffer[sp]: acc->propBufferNew[sp];
                acc->srcReRun[sp].set_arg(0, reBuf);
                acc->srcReRun[sp].start();
                acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
                acc->gsRun[sp].set_arg(2, info->chunkProp[p][sp].edgeNumChunk * 2);
                acc->gsRun[sp].start();
            }
            acc->writeRun.set_arg(0, acc->propTemp);
            acc->writeRun.set_arg(2, info->scheduleItem[p] * PARTITION_SIZE); // offset 
            acc->writeRun.set_arg(3, PARTITION_SIZE); // vertex number
            acc->writeRun.start();

            auto applyBuf = (pingpong == 0)? acc->propBuffer[3]: acc->propBufferNew[3];
            acc->applyRun.set_arg(0, applyBuf); // apply in SLR0
            acc->applyRun.set_arg(5, (info->scheduleItem[p] * PARTITION_SIZE)); // vertex offset
            acc->applyRun.set_arg(6, PARTITION_SIZE); // vertex number in each partition
            acc->applyRun.set_arg(7, 0);
            acc->applyRun.start();

            for (int sp = 0; sp < KERNEL_NUM; sp++) {
                acc->srcReRun[sp].wait();
                acc->gsRun[sp].wait();
            }
            acc->writeRun.wait();
            acc->applyRun.wait();

        } else if (p == schedule_num) {
            // std::cout << world_rank << "] => last partition " << std::endl;
            // last partition
            acc->rdPropRun.set_arg(0, acc->propTemp);
            acc->rdPropRun.set_arg(2, world_rank);
            acc->rdPropRun.set_arg(3, 1);
            acc->rdPropRun.set_arg(4, info->scheduleItem[p-1] * PARTITION_SIZE); // read offset
            acc->rdPropRun.set_arg(5, PARTITION_SIZE); // transmission data size
            acc->rdPropRun.start();

            acc->rdPropRun.wait();
            for (int sp = 0; sp < KERNEL_NUM; sp++) {
                acc->wrPropRun[sp].wait();
            }

        } else {
            // std::cout << world_rank << "] => partition " << p << " " << info->scheduleItem[p] << std::endl;
            for (int sp = 0; sp < KERNEL_NUM; sp++) {
                auto reBuf = (pingpong == 0)? acc->propBuffer[sp]: acc->propBufferNew[sp];
                acc->srcReRun[sp].set_arg(0, reBuf);
                acc->srcReRun[sp].start();
                acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
                acc->gsRun[sp].set_arg(2, info->chunkProp[p][sp].edgeNumChunk * 2);
                acc->gsRun[sp].start();
            }
            acc->writeRun.set_arg(0, acc->propTemp);
            acc->writeRun.set_arg(2, info->scheduleItem[p] * PARTITION_SIZE); // offset 
            acc->writeRun.set_arg(3, PARTITION_SIZE); // vertex number
            acc->writeRun.start();

            auto applyBuf = (pingpong == 0)? acc->propBuffer[3]: acc->propBufferNew[3];
            acc->applyRun.set_arg(0, applyBuf);
            acc->applyRun.set_arg(5, (info->scheduleItem[p] * PARTITION_SIZE)); // vertex offset
            acc->applyRun.set_arg(6, PARTITION_SIZE); // vertex number in each partition
            acc->applyRun.set_arg(7, 0);
            acc->applyRun.start();

            acc->rdPropRun.set_arg(0, acc->propTemp);
            acc->rdPropRun.set_arg(2, world_rank);
            acc->rdPropRun.set_arg(3, 1);
            acc->rdPropRun.set_arg(4, info->scheduleItem[p-1] * PARTITION_SIZE); // read offset
            acc->rdPropRun.set_arg(5, PARTITION_SIZE); // transmission data size
            acc->rdPropRun.start();

            for (int sp = 0; sp < KERNEL_NUM; sp++) {
                acc->srcReRun[sp].wait();
                acc->gsRun[sp].wait();
            }
            // std::cout << "GS kernel done" << std::endl;
            acc->writeRun.wait();
            acc->applyRun.wait();
            // std::cout << "write/apply kernel done" << std::endl;
            acc->rdPropRun.wait();
            // std::cout << "read kernel done" << std::endl;
        }

        // auto end_p = std::chrono::steady_clock::now();
        // int time_p = std::chrono::duration_cast<std::chrono::microseconds>(end_p - start_p).count();
        // if (p < schedule_num) {
        //     std::cout << "partition " << info->scheduleItem[p] << " time = " << time_p << " us " << std::endl;
        // } else {
        //     std::cout << "partition last time = " << time_p << " us " << std::endl;
        // }
    }

    return 0;
}


int resultTransfer(graphInfo *info, graphAccelerator * acc, int run_counter)
{
    // Transfer device buffer content to host side
    int world_rank = -1;
    int world_size = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    acc->outRegBuffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    acc->propTemp.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    for (int sp = 0; sp < KERNEL_NUM; sp++) {
        acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    /*
    
    std::ofstream outputFile("./run_verify/clementi_" + std::to_string(world_size) + "_node.bin", std::ios::binary);
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

    log_trace("[INFO] Sync data (temp, outReg) to host ");

    return 0;
}

