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
#include <cmath>
#include <iomanip>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

#define GRAPH_DATASET_DIRETORY "/data/yufeng/single_4SLR_graph_dataset_bp2/"
#define USE_PERF true

#define SUB_PARTITION_NUM 4

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

    sda::utils::CmdLineParser parser;
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.parse(argc, argv);
    std::string g_name = parser.value("dataset");

    // std::string xclbin_file = "./build_dir_hw_single_gp_/kernel.link.xclbin";
    std::string xclbin_file = "./mem_burst_len_32/build_dir_hw_single_gp_/kernel.link.xclbin";
    xrt::device graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    xrt::uuid graphUuid = graphDevice.load_xclbin(xclbin_file);
    std::cout << "load xclbin done" << std::endl;

    // acceleratorDataLoad(g_name, &graphDataInfo);
    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    int num_partition = getPartitionNum(directory + g_name, SUB_PARTITION_NUM);
    std::cout << "num_partition = " << num_partition << std::endl;
    // int num_partition = 1;

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

    // load map data and initialize prop data
    std::ifstream mapFileReader(directory + g_name + "/idxMap.bin", std::ios::binary);
    int original_vertex_num = -1;
    mapFileReader.read((char*)&original_vertex_num, sizeof(int));
    mapFileReader.close();
    std::cout << "original num = " << original_vertex_num << std::endl;

    std::ifstream degFileReader(directory + g_name + "/outDeg.bin", std::ios::binary);
    int vertex_num = -1;
    degFileReader.read((char*)&vertex_num, sizeof(int));
    int* outDegArray = new int [vertex_num];
    degFileReader.read((char*)outDegArray, sizeof(int) * vertex_num);
    degFileReader.close();
    int aligned_vertex_num = ((vertex_num + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
    int* prop_array = new int [aligned_vertex_num];
    xrt::bo prop_buf = xrt::bo(graphDevice, aligned_vertex_num * sizeof(int) , cr_kernel.group_id(0));
    prop_array = prop_buf.map<int*>();
    for (int i = 0; i < aligned_vertex_num; i++) {
        if (i < vertex_num) {
            float temp = 1.0f / original_vertex_num;
            prop_array[i] = std::pow(2,30) * temp;
            prop_array[i] = prop_array[i] / outDegArray[i];
        } else {
            prop_array[i] = 0;
        }
    }

/*
    int original_vertex_num = 64*1024*1024;
    int vertex_num = original_vertex_num;
    int aligned_vertex_num = ((vertex_num + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
    int* prop_array = new int [aligned_vertex_num];
    xrt::bo prop_buf = xrt::bo(graphDevice, aligned_vertex_num * sizeof(int) , cr_kernel.group_id(0));
    prop_array = prop_buf.map<int*>();
    for (int i = 0; i < aligned_vertex_num; i++) {
        if (i < vertex_num) {
            prop_array[i] = 1;
        } else {
            prop_array[i] = 0;
        }
    }
*/

    std::vector<int> number_edge;
    std::vector<int> number_vertex;
    std::vector<int> time_device;
    std::vector<int> time_host;
    std::vector<int> mteps;
    number_edge.resize(num_partition * SUB_PARTITION_NUM);
    number_vertex.resize(num_partition * SUB_PARTITION_NUM);
    time_device.resize(num_partition * SUB_PARTITION_NUM);
    time_host.resize(num_partition * SUB_PARTITION_NUM);
    mteps.resize(num_partition);
    std::cout << "sub_partition_num = " << SUB_PARTITION_NUM << std::endl;

    for (int p = 0; p < num_partition; p++) {
    // for (int p = 1; p < 2; p++) {
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            // load edge array
            std::string edge_txt_name = directory + g_name + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".bin";
            // std::cout << "load local dataset " << std::endl;
            std::cout << " sp = " << sp << " ";
            // std::string edge_txt_name = "./8-9-debug-largegraph/test_graph/test_vertex_" + std::to_string(sp) + ".bin";
            std::ifstream edgeFileReader(edge_txt_name, std::ios::binary);
            int edge_num = -1;
            edgeFileReader.read((char*)&edge_num, sizeof(int));
            edgeFileReader.read((char*)&edge_num, sizeof(int)); // read twice, second value is reserved for future use.
            std::cout << " edge number = " << edge_num;
            int* edge_array = new int[edge_num * 2];
            // create the device buffer
            xrt::bo edge_buf = xrt::bo(graphDevice, edge_num * 2 * sizeof(int) , gs_kernel.group_id(0));
            edge_array = edge_buf.map<int*>();
	        edgeFileReader.read((char*)edge_array, edge_num * 2 * sizeof(int));
            edgeFileReader.close();
            number_edge[p*SUB_PARTITION_NUM + sp] = edge_num;
            number_vertex[p*SUB_PARTITION_NUM + sp] = edge_array[edge_num * 2 - 1] - edge_array[0]; // get vertex number
            // std::cout << "[" << p << "][" << sp << "] edge_num = " << edge_num;

#if USE_PERF==true
            xrt::bo perf_buf = xrt::bo(graphDevice, 1 * sizeof(int) , wr_kernel.group_id(5));
            int* perf_count;
            perf_count = perf_buf.map<int*>();
#endif
            
            xrt::bo out_buf = xrt::bo(graphDevice, aligned_vertex_num * sizeof(int) , wr_kernel.group_id(0));
            int* out_array = new int[aligned_vertex_num];
            out_array = out_buf.map<int*>();

            // std::cout << "[" << p << "][" << sp << "] map done " << std::endl;

            // sync data to device
#if USE_PERF==true
            perf_count[0] = 0;
            perf_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#endif
            edge_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            prop_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            // out_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            // std::cout << "[" << p << "][" << sp << "] sync done " << std::endl;
            
            // set args 
            gs_run.set_arg(0, edge_buf);
            gs_run.set_arg(2, edge_num*2);
            gs_run.set_arg(3, 0);
            gs_run.set_arg(4, PARTITION_SIZE); // should be dest vertex number, but I align, need modify
            cr_run.set_arg(0, prop_buf);
            wr_run.set_arg(0, out_buf);
            wr_run.set_arg(2, p * PARTITION_SIZE); 
            wr_run.set_arg(3, PARTITION_SIZE);
#if USE_PERF==true
            wr_run.set_arg(5, perf_buf);
#endif

            auto start_kernel = chrono::steady_clock::now();
            // kernel run and wait
            wr_run.start();
            cr_run.start();
            gs_run.start();

            gs_run.wait();
            cr_run.wait();
            wr_run.wait();
            auto end_kernel = chrono::steady_clock::now();
            int host_time = chrono::duration_cast<chrono::microseconds>(end_kernel - start_kernel).count();

            // result transfer
            out_buf.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            // for (int i = 0; i < aligned_vertex_num; i++) {
            //     std::cout << out_array[i] << " ";
            //     if ((i+1) % 50 == 0) std::cout << std::endl;
            // }

#if USE_PERF==true
            perf_buf.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            int perf_num = perf_count[0];
            int time_us = perf_num / 250; // kernel run at 250 Mhz
            time_device[p * SUB_PARTITION_NUM + sp] = time_us;
            time_host[p * SUB_PARTITION_NUM + sp] = host_time;
            // std::cout << ", device time = " << time_us << " us, host time = " << host_time << " us"<< std::endl;
#endif
        }
        std::cout << std::endl;
    }

    // for (int p = 0; p < num_partition; p++) {
    //     std::cout << std::setw(10) << "[" << p <<  "]  edge number = ";
    //     for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //         std::cout << std::setw(6) << number_edge[p * SUB_PARTITION_NUM + sp] << " ";
    //     }
    //     std::cout << std::endl;
    // }

    /*
    for (int p = 0; p < num_partition; p++) {
        std::cout << std::setw(10) << "[" << p <<  "]  device  time = ";
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            int time = time_device[p * SUB_PARTITION_NUM + sp];
            std::cout << std::setw(6) << time_device[p * SUB_PARTITION_NUM + sp]<< " ";
        }
        std::cout << " us" << std::endl;
    }

    int nodeTimes[4] = {0};
    sort(nodeTimes, nodeTimes + 4);
    for (int i = 0; i < num_partition; i += 4) {
        double current_node[4];
        int time_temp[4];
        for (int j = 0; j < 4; j++) {
            time_temp[j] = time_device[i + j];
            current_node[j] = 3.096e+05 + 0.6971 * number_edge[i * SUB_PARTITION_NUM + j] + \
                            0.2648 * number_vertex[i * SUB_PARTITION_NUM + j] + \
                            (-1.076e-08) * number_edge[i * SUB_PARTITION_NUM + j] * number_vertex[i * SUB_PARTITION_NUM + j] \
                            + 4.187e-08 * number_vertex[i * SUB_PARTITION_NUM + j] * number_vertex[i * SUB_PARTITION_NUM + j];
        }

        int indexes[4];
        for (int j = 0; j < 4; j++) {
            indexes[j] = j;
        }

        sort(indexes, indexes + 4, [&](int a, int b) { return current_node[a] < current_node[b]; });
        // sort(current_node, current_node + 4);
        for (int j = 3; j >= 0; j--) {
            // nodeTimes[j] += current_node[3 - j];
            nodeTimes[j] += time_temp[indexes[3 - j]];
        }
        sort(nodeTimes, nodeTimes + 4);
    }
    for (int i = 0; i < 4; i++) {
        cout << "Node " << i + 1 << ": " << nodeTimes[i] << endl;
    }
    */

    // std::cout << std::setw(6) << " workload imbalance = " << total_max_min << " us" << std::endl;
    
    std::ofstream outfile("/home/yufeng/gp_temp/fpga/wb_partition/m_access/" + g_name + "_MTEPS.txt");
    for (int p = 0; p < num_partition; p++) {
        int avg_mteps = 0;

        std::cout << std::setw(10) << "[" << p <<  "]  device MTEPS = ";
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            avg_mteps += number_edge[p * SUB_PARTITION_NUM + sp] / (time_device[p * SUB_PARTITION_NUM + sp] + 1);
            std::cout << std::setw(6) << number_edge[p * SUB_PARTITION_NUM + sp] / (time_device[p * SUB_PARTITION_NUM + sp] + 1) << " ";
        }
        mteps[p] = avg_mteps / SUB_PARTITION_NUM;
        std::cout << " MTEPS " << std::endl;
    }

    if (!outfile.is_open()) {
        std::cerr << "Failed to open mteps output file." << std::endl;
        return 1;
    }
    for (int value : mteps) {
        outfile << value << std::endl;
    }
    outfile.close();


    for (int p = 0; p < num_partition; p++) {
        std::cout << std::setw(10) << "[" << p <<  "]  device  time = ";
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            int time = time_device[p * SUB_PARTITION_NUM + sp];
            std::cout << std::setw(6) << time_device[p * SUB_PARTITION_NUM + sp];
        }
        std::cout << " us" << std::endl;
    }

    for (int p = 0; p < num_partition; p++) {
        std::cout << std::setw(10) << "[" << p <<  "]  host  time = ";
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            int time = time_host[p * SUB_PARTITION_NUM + sp];
            std::cout << std::setw(6) << time_host[p * SUB_PARTITION_NUM + sp];
        }
        std::cout << " us" << std::endl;
    }

    
    // for (int p = 0; p < num_partition; p++) {

    //     std::cout << std::setw(10) << "[" << p <<  "]  host MTEPS = ";
    //     for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //         std::cout << std::setw(6) << number_edge[p * SUB_PARTITION_NUM + sp] / (time_host[p * SUB_PARTITION_NUM + sp] + 1) << " ";
    //     }
    //     std::cout << " MTEPS " << std::endl;

    // }

}
