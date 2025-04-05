#include "../cmdparser/cmdlineparser.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mpi.h>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

// ######### Should change this diretory in your environment #########
#define GRAPH_DATASET_DIRETORY "/data/yufeng/single_4SLR_graph_dataset_bp2/"
#define XCLBIN_DIRETORY "./build_dir_hw_gather_scatter_pr/kernel.link.xclbin"
// ###################################################################

#define USE_PERF true

int getPartitionNum (std::string dataset_name, int sub_partition_num) {
    int p_index = 0;
    int sp_index = 0;
    for (int p = 0; p < 100; p++) {
        for (int sp = 0; sp < 100; sp ++) {
            std::string file_name = dataset_name + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".bin";
            std::ifstream find_file(file_name.c_str());
            if (find_file.good()) {
                p_index = (p > p_index)? p: p_index;
                sp_index = (sp > sp_index)? sp: sp_index;
            }
        }
    }
    if (sub_partition_num != (sp_index + 1)) { // double check input txt file.
        std::cout << "sub_partition_num = " << sub_partition_num << " sp_index = " << sp_index << std::endl;
    }
    return (p_index + 1);
}

int main(int argc, char** argv) {

    std::cout << "Please modify the GRAPH_DATASET_DIRETORY into your path ! " << std::endl;
    std::cout << "Current GRAPH_DATASET_DIRETORY : " << GRAPH_DATASET_DIRETORY << std::endl;
    std::cout << "Current XCLBIN_DIRETORY : " << XCLBIN_DIRETORY << std::endl;

    MPI_Init(NULL, NULL); // init for multi-node
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    std::cout << "World Rank = " << world_rank << "/" << world_size << std::endl;

    sda::utils::CmdLineParser parser;
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.parse(argc, argv);
    std::string g_name = parser.value("dataset");

    std::string xclbin_file = XCLBIN_DIRETORY;
    xrt::device graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    xrt::uuid graphUuid = graphDevice.load_xclbin(xclbin_file);
    std::cout << "load xclbin done" << std::endl;

    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    int num_partition = getPartitionNum(directory + g_name, SUB_PARTITION_NUM);

    std::string krnl_name; // for kernel initialize
    krnl_name = "gs:{gs_1}";
    xrt::kernel gs_kernel = xrt::kernel(graphDevice, graphUuid, krnl_name.c_str());
    krnl_name = "src_cache_read_engine:{src_cache_read_engine_1}";
    xrt::kernel cr_kernel = xrt::kernel(graphDevice, graphUuid, krnl_name.c_str());
    krnl_name = "streamWrite:{streamWrite_1}";
    xrt::kernel wr_kernel = xrt::kernel(graphDevice, graphUuid, krnl_name.c_str());

    xrt::run gs_run = xrt::run(gs_kernel);
    xrt::run cr_run = xrt::run(cr_kernel);
    xrt::run wr_run = xrt::run(wr_kernel);

    int sub_par_per_node = SUB_PARTITION_NUM/PROCESSOR_NUM;
    for (int p = 0; p < num_partition; p++) {
        for (int sp = 0; sp < sub_par_per_node; sp++) {
            // load edge array
            int sp_idx = sp * PROCESSOR_NUM + world_rank;
            std::string edge_txt_name = directory + g_name + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp_idx) + ".bin";
            std::ifstream edgeFileReader(edge_txt_name, std::ios::binary);
            int edge_num = -1;
            edgeFileReader.read((char*)&edge_num, sizeof(int));
            edgeFileReader.read((char*)&edge_num, sizeof(int)); // read twice, second value is reserved for future use.
            int* edge_array = new int[edge_num * 2];
            // create the device buffer
            xrt::bo edge_buf = xrt::bo(graphDevice, edge_num * 2 * sizeof(int) , gs_kernel.group_id(0));
            edge_array = edge_buf.map<int*>();
	        edgeFileReader.read((char*)edge_array, edge_num * 2 * sizeof(int));
            edgeFileReader.close();
            // std::cout << "[" << p << "][" << sp_idx << "] edge_num = " << edge_num << std::endl;

#if USE_PERF==true
            xrt::bo perf_buf = xrt::bo(graphDevice, 1 * sizeof(int) , wr_kernel.group_id(5));
            int* perf_count;
            perf_count = perf_buf.map<int*>();
#endif

            // load map data and initialize prop data
            std::ifstream mapFileReader(directory + g_name + "/idxMap.bin", std::ios::binary);
            int vertex_num = -1;
            mapFileReader.read((char*)&vertex_num, sizeof(int));
            mapFileReader.read((char*)&vertex_num, sizeof(int)); // the second is the compressed vertex.
            int aligned_vertex_num = ((vertex_num + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
            int* prop_array = new int [aligned_vertex_num];
            xrt::bo prop_buf = xrt::bo(graphDevice, aligned_vertex_num * sizeof(int) , cr_kernel.group_id(0));
            prop_array = prop_buf.map<int*>();
            for (int i = 0; i < aligned_vertex_num; i++) {
                if (i < vertex_num) {
                    prop_array[i] = 100;
                } else {
                    prop_array[i] = 0;
                }
            }
            
            xrt::bo out_buf = xrt::bo(graphDevice, PARTITION_SIZE * sizeof(int) , wr_kernel.group_id(0));
            int* out_array = new int[PARTITION_SIZE];
            out_array = out_buf.map<int*>();
            for (int i = 0; i < PARTITION_SIZE; i++) {
                out_array[i] = i;
            }

            // sync data to device
#if USE_PERF==true
            perf_count[0] = 0;
            perf_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#endif
            edge_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            prop_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            out_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            
            // set args 
            gs_run.set_arg(0, edge_buf);
            gs_run.set_arg(2, edge_num);
            gs_run.set_arg(3, 0);
            gs_run.set_arg(4, PARTITION_SIZE); // should be dest vertex number, but I align, need modify
            cr_run.set_arg(0, prop_buf);
            wr_run.set_arg(0, out_buf);
            wr_run.set_arg(2, 0); // offset 
            wr_run.set_arg(3, PARTITION_SIZE); // vertex number
#if USE_PERF==true
            wr_run.set_arg(5, perf_buf);
#endif

            MPI_Barrier(MPI_COMM_WORLD);
            auto start_kernel = chrono::steady_clock::now();
            // kernel run and wait
            wr_run.start();
            cr_run.start();
            gs_run.start();

            gs_run.wait();
            cr_run.wait();
            wr_run.wait();
            auto end_kernel = chrono::steady_clock::now();
            MPI_Barrier(MPI_COMM_WORLD);
            int kernel_time = chrono::duration_cast<chrono::microseconds>(end_kernel - start_kernel).count();

            // result transfer
            out_buf.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
#if USE_PERF==true
            perf_buf.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            int perf_num = perf_count[0];
            int time_us = perf_num / 250; // kernel run at 250 Mhz
            std::cout << "run on node_" << world_rank << ", [" << p << "][" << sp_idx << "] kernel time = " << kernel_time << \
                        " us " << std::endl;
#endif

        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
