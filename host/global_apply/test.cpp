#include "../cmdparser/cmdlineparser.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <mpi.h>
#include <thread>
#include <atomic>

#include "../network/network.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

// 1 for using one node, 0 for using multi nodes concurrently
#define ONE_PARALLEL 0

std::map<int, std::map<std::string, std::string>> FPGA_config = \
   {{0 , {{"ip_addr" , "192.168.0.201"}, {"tx_port" , "60510"}, {"rx_port" , "5001"}, {"idx" , "201"}, {"MAC_addr" , "00:0a:35:02:9d:c8"}}}, \
    {1 , {{"ip_addr" , "192.168.0.202"}, {"tx_port" , "60511"}, {"rx_port" , "5001"}, {"idx" , "202"}, {"MAC_addr" , "00:0a:35:02:9d:c9"}}}, \
    {2 , {{"ip_addr" , "192.168.0.203"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "203"}, {"MAC_addr" , "00:0a:35:02:9d:ca"}}}, \
    {3 , {{"ip_addr" , "192.168.0.204"}, {"tx_port" , "60513"}, {"rx_port" , "5001"}, {"idx" , "204"}, {"MAC_addr" , "00:0a:35:02:9d:cb"}}}, \
    {4 , {{"ip_addr" , "192.168.0.205"}, {"tx_port" , "60514"}, {"rx_port" , "5001"}, {"idx" , "205"}, {"MAC_addr" , "00:0a:35:02:9d:cc"}}}, \
    {5 , {{"ip_addr" , "192.168.0.206"}, {"tx_port" , "60515"}, {"rx_port" , "5001"}, {"idx" , "206"}, {"MAC_addr" , "00:0a:35:02:9d:cd"}}}, \
    {6 , {{"ip_addr" , "192.168.0.207"}, {"tx_port" , "60516"}, {"rx_port" , "5001"}, {"idx" , "207"}, {"MAC_addr" , "00:0a:35:02:9d:ce"}}}, \
    {7 , {{"ip_addr" , "192.168.0.208"}, {"tx_port" , "60517"}, {"rx_port" , "5001"}, {"idx" , "208"}, {"MAC_addr" , "00:0a:35:02:9d:cf"}}}};


std::atomic<bool> stopFlag(false); // Atomic flag to stop the extra thread

void registerReaderThread(AlveoVnxNetworkLayer& networkLayer, uint32_t current_packet, int node_id) {
    while (!stopFlag) {
        uint32_t packet_num = networkLayer.readRegister_offset("pkth_in_packets", 0);
        packet_num = packet_num - current_packet;
        std::cout << node_id << " node, packet number = " << packet_num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1)); // read register at 1s interval
    }
}


int main(int argc, char** argv) {

    int data_number = (16*21*3120 + 16*16);  // 1M vertex = 16*21*3120 + 16*16

    sda::utils::CmdLineParser parser;
    parser.addSwitch("--super_step", "-s", "super step number", "5"); // set super-step number as 5
    parser.parse(argc, argv);

    log_set_level(LOG_INFO); // set log level LOG_INFO;

    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    std::cout << "MPI init done:" << world_rank << " / " << world_size << std::endl;

    if (world_size != PROCESSOR_NUM) {
        std::cout << "world_size = "  << world_size << " PROCESS_NUM = " << PROCESSOR_NUM << std::endl;
        std::cout << "MPI number does not equal to setting !" << std::endl;
        return 0;
    }

    std::string xclbin_file = "./build_dir_hw_global_apply/kernel.link.xclbin";
    xrt::device graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    xrt::uuid graphUuid = graphDevice.load_xclbin(xclbin_file);
    std::cout << "load xclbin done" << std::endl;

    std::string krnl_name; // for kernel initialize
    krnl_name = "writeProp:{writeProp_1}";
    xrt::kernel write_kernel = xrt::kernel(graphDevice, graphUuid, krnl_name.c_str());
    krnl_name = "readProp:{readProp_1}";
    xrt::kernel read_kernel = xrt::kernel(graphDevice, graphUuid, krnl_name.c_str());

    xrt::run read_run = xrt::run(read_kernel);
    xrt::run write_run = xrt::run(write_kernel);
    std::cout << "kernel initialize done " << std::endl; 

    // network kernel initialize and status check.

    bool linkStatus = false;
    AlveoVnxCmac cmac_0 = AlveoVnxCmac(graphDevice, graphUuid, 0);
    linkStatus = cmac_0.readRegister("stat_rx_status") & 0x1;
    while(!linkStatus) {
        sleep(1);
        linkStatus = cmac_0.readRegister("stat_rx_status") & 0x1;
        std::cout << "[INFO] Processor " << world_rank << ", check linkstatus ..." << std::endl;
    }
    std::cout << "[INFO] Processor " << world_rank << ", check linkstatus ...  done" << std::endl;

    // set socket table, run arp discovery;
    // upper worker => lower worker: 3->0, 0->1, 1->2, 2->3;

    
    int upper_worker_idx = (world_rank == 0)? (world_size - 1) : (world_rank - 1);
    int lower_worker_idx = (world_rank == (world_size - 1))? 0 : (world_rank + 1);

    AlveoVnxNetworkLayer netlayer_0 = AlveoVnxNetworkLayer(graphDevice, graphUuid, 0);
    netlayer_0.setAddress(FPGA_config[world_rank]["ip_addr"], FPGA_config[world_rank]["MAC_addr"]);
    netlayer_0.setSocket(FPGA_config[upper_worker_idx]["ip_addr"], stoi(FPGA_config[upper_worker_idx]["tx_port"]), 5001, 0); // set recv socket
    netlayer_0.setSocket(FPGA_config[lower_worker_idx]["ip_addr"], 5001, stoi(FPGA_config[world_rank]["tx_port"]), 1); // set send socket
    netlayer_0.getSocketTable();

    bool ARP_ready = false;
    std::cout << "Processor " << world_rank << " , wait ARP ... " << std::endl;
    while(!ARP_ready) {
        netlayer_0.runARPDiscovery();
        usleep(500000);
        ARP_ready = netlayer_0.IsARPTableFound(FPGA_config[lower_worker_idx]["ip_addr"]);
    }
    std::cout << "Processor " << world_rank << " , wait ARP ... done" << std::endl;    
    
    /*

    int upper_worker_idx = (world_rank == 0)? (world_size - 1) : (world_rank - 1);
    int lower_worker_idx = (world_rank == (world_size - 1))? 0 : (world_rank + 1);

    AlveoVnxNetworkLayer netlayer_0 = AlveoVnxNetworkLayer(graphDevice, graphUuid, 0);
    AlveoVnxNetworkLayer netlayer_1 = AlveoVnxNetworkLayer(graphDevice, graphUuid, 1);

    netlayer_0.setAddress(FPGA_config[world_rank*2+1]["ip_addr"], FPGA_config[world_rank*2+1]["MAC_addr"]);
    // netlayer_0.setSocket(FPGA_config[lower_worker_idx*2]["ip_addr"], stoi(FPGA_config[lower_worker_idx*2]["tx_port"]), 5001, 0); // set recv socket
    netlayer_0.setSocket(FPGA_config[lower_worker_idx*2]["ip_addr"], 5001, stoi(FPGA_config[world_rank*2+1]["tx_port"]), 1); // set send socket
    netlayer_0.getSocketTable();

    netlayer_1.setAddress(FPGA_config[world_rank*2]["ip_addr"], FPGA_config[world_rank*2]["MAC_addr"]);
    netlayer_1.setSocket(FPGA_config[upper_worker_idx*2+1]["ip_addr"], stoi(FPGA_config[upper_worker_idx*2+1]["tx_port"]), 5001, 1); // set recv socket
    // netlayer_1.setSocket(FPGA_config[upper_worker_idx*2+1]["ip_addr"], 5001, stoi(FPGA_config[world_rank*2]["tx_port"]), 1); // set send socket
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

    */

    int* vertex_prop = new int [data_number * world_size];
    xrt::bo prop_buf = xrt::bo(graphDevice, data_number * world_size * sizeof(int) , read_kernel.group_id(0));
    vertex_prop = prop_buf.map<int*>();
    int* update_prop = new int [data_number * world_size]; // should receive data from each node;
    xrt::bo prop_recv = xrt::bo(graphDevice, data_number * world_size * sizeof(int) , write_kernel.group_id(0));
    update_prop = prop_recv.map<int*>();

    int* vertex_prop_ref = new int [data_number * world_size];

#if ONE_PARALLEL == 1
    for (int i = 0; i < data_number * world_size; i++) {
        if ((i >= (0 * data_number)) && (i < (0 + 1) * data_number)) {
           vertex_prop[i] = i;
           vertex_prop_ref[i] = i;
        } else {
           vertex_prop[i] = 0;
           vertex_prop_ref[i] = 0;
        }
        update_prop[i] = 0;
    }
#else
    for (int i = 0; i < data_number * world_size; i++) {
        if ((i >= (world_rank * data_number)) && (i < (world_rank + 1) * data_number)) {
           vertex_prop[i] = i;
        } else {
           vertex_prop[i] = 0;
        }
        vertex_prop_ref[i] = i;
        update_prop[i] = 0;
    }
#endif

    prop_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    prop_recv.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    std::cout << world_rank << " data allocate and sync to device buffer done" << std::endl;

    MPI_Barrier(MPI_COMM_WORLD); // need a barrier to sync;

    // set args 
    // read kernel : unsigned int node_id, unsigned int dest_id, unsigned int vertexOffset, unsigned int vertexNum)
    // write kernel : unsigned int packet_num
    int read_offset = world_rank * data_number;

#if ONE_PARALLEL == 1
    int packet_num = (((data_number >> 4) + 20) / 21);
#else 
    int packet_num = world_size * (((data_number >> 4) + 20) / 21);
#endif

    std::cout << "read_offset = " << read_offset << std::endl;
    std::cout << "packet_num = " << packet_num << std::endl;

    // if ((world_rank == 2) || (world_rank == 0)) {
    write_run.set_arg(0, prop_recv);
    write_run.set_arg(2, packet_num);
    write_run.start();
    // }

    // detach another thread to read packet number
    uint32_t rx_packet_num_start = netlayer_0.readRegister_offset("pkth_in_packets", 0); // lsb
    std::thread extraThread(registerReaderThread, std::ref(netlayer_0), rx_packet_num_start, world_rank);
    extraThread.detach(); // Detach the extra thread

    std::this_thread::sleep_for(std::chrono::seconds(2));

    MPI_Barrier(MPI_COMM_WORLD); // need a barrier to get accurate perf number;

#if ONE_PARALLEL == 1
    if (world_rank == 0) {
        read_run.set_arg(0, prop_buf);
        read_run.set_arg(2, world_rank);
        read_run.set_arg(3, 1);
        read_run.set_arg(4, read_offset); // should be considered with world_rank
        read_run.set_arg(5, data_number); // transmission data size
        read_run.start();
    }
#else 
    read_run.set_arg(0, prop_buf);
    read_run.set_arg(2, world_rank);
    read_run.set_arg(3, 1);
    read_run.set_arg(4, read_offset); // should be considered with world_rank
    read_run.set_arg(5, data_number); // transmission data size
    read_run.start();
#endif 

    std::cout << world_rank << " kernel start done" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

#if ONE_PARALLEL == 1
    if (world_rank == 0) {
        read_run.wait();
    }
#else
    read_run.wait();
#endif

    write_run.wait();

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << world_rank << " kernel execute time = " << duration.count() << " microseconds" << std::endl;

    // Sleep briefly to allow the extra thread to stop 
    std::this_thread::sleep_for(std::chrono::seconds(2));
    stopFlag = true;

    prop_recv.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    MPI_Barrier(MPI_COMM_WORLD); // need a barrier to sync;

    // check results, gather data from other node;
    std::vector<int> receiveArray(data_number * world_size * world_size);
    MPI_Gather(update_prop, data_number * world_size, MPI_INT, receiveArray.data(), data_number * world_size, MPI_INT, 0, MPI_COMM_WORLD);

    bool correct = true;
    if (world_size == 1) {
        std::cout << std::boolalpha << "result alignment = " << correct << std::endl;
    } else if (world_rank == 0) {
        for (int idx = 0; idx < data_number * world_size; idx++) {
            for (int node_id = 0; node_id < world_size; node_id++) {
                int temp_a = receiveArray[node_id * data_number * world_size + idx];
                int temp_b = vertex_prop_ref[idx];
                if (temp_a != temp_b) {
                    correct = false;
                    std::cout << "node id = " << node_id << " data not equal with node id = " \
                    << (node_id + 1) << " on index " << idx << std::endl;
                }
            }
        }
        std::cout << std::boolalpha << "result alignment = " << correct << std::endl;
    }

    delete[] vertex_prop_ref;

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
