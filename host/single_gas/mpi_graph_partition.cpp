
// #include "host_graph_sw.h"
// #include "host_graph_scheduler.h"
#include <cstring>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "mpi_graph_api.h"
#include "mpi_host.h"
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

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
        log_error("[ERROR] SUBPARTITION TXT FILE NOT ALIGN !");
        std::cout << "sub_partition_num = " << sub_partition_num << " sp_index = " << sp_index << std::endl;
    }
    return (p_index + 1);
}

int acceleratorDataLoad(const std::string &gName, graphInfo *info)
{
    // std::string directory = GRAPH_DATASET_DIRETORY + std::to_string(SUB_PARTITION_NUM) + "/";
    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    log_debug("Load graph data from directory : %s", directory.c_str());

    int num_partition = getPartitionNum(directory + gName, SUB_PARTITION_NUM);
    info->partitionNum = num_partition;

    std::ifstream mapFileReader(directory + gName + "/idxMap.bin", std::ios::binary);
    std::ifstream outDegReader(directory + gName + "/outDeg.bin", std::ios::binary);
	if ((!mapFileReader)||(!outDegReader))
	{
		log_error("[ERROR] Read Map or outDeg file failed !");
		return -1;
	}
    int vertex_num = -1;
    mapFileReader.read((char*)&vertex_num, sizeof(int)); // first value
    info->vertexNum = vertex_num;
    int num_mapped = -1;
    mapFileReader.read((char*)&num_mapped, sizeof(int)); // second value
    int num_out_deg = -1;
    outDegReader.read((char*)&num_out_deg, sizeof(int)); // first value

    if ((vertex_num < 0)||(num_mapped < 0)||(num_out_deg < 0)) {
        log_error("[ERROR] Read Map and outDeg file failed !");
    }
    if (num_mapped != num_out_deg) {
        log_error("[ERROR] Read Map and outDeg number not aligned !");
    }
    int* map_array = new int[vertex_num];
	mapFileReader.read((char*)map_array, vertex_num * sizeof(int));
    int* out_deg_array = new int[num_mapped];
    outDegReader.read((char*)out_deg_array, num_mapped * sizeof(int));
    int temp_index = 0;
    for (int i = 0; i < vertex_num; i++) {
        if (map_array[i] < 0) {
            info->outDegZeroIndex.push_back(i);
        } else {
            info->outDeg.push_back(out_deg_array[temp_index]);
            info->vertexMapping.push_back(map_array[i]);
            temp_index ++;
        }
    }
    mapFileReader.close();
    outDegReader.close();
    delete []map_array;
    delete []out_deg_array;
    log_trace("[TRACE] load idxMap & outDeg files done! ");


    // read scheduler order file
    if (USE_SCHEDULER == true) {
        log_trace("[INFO] task scheduler, load order file ... ");
        info->order.resize(info->partitionNum);
        for (int i = 0; i < info->partitionNum; i++) {
            info->order[i].resize(SUB_PARTITION_NUM);
        }
        std::fstream order_file;
        order_file.open(directory + gName + "/order.txt", std::ios::in);
        if (order_file.is_open()) {
            std::string temp_line, temp_word;
            int line = 0;
            while(getline(order_file, temp_line)) {
                std::stringstream ss(temp_line);
                int i = 0;
                while (getline(ss, temp_word, ' ')) {
                    info->order[line][i] = stoi(temp_word);
                    i += 1;
                }
                line += 1;
            }
        }
        order_file.close();
        log_trace("[TRACE] load scheduler order files done! ");
    }


    // Allocate edge mem space in Host side
    info->compressedVertexNum = num_mapped;
    int aligned_vertex_num = ((num_mapped + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
    info->alignedCompressedVertexNum = aligned_vertex_num;
    log_debug("whole graph compressed vertex num %d, algned %d", num_mapped, aligned_vertex_num);


    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    info->chunkProp.resize(num_partition);
    info->chunkEdgeData.resize(num_partition);
    info->chunkPropData.resize(kernel_per_node);
    info->chunkPropDataNew.resize(kernel_per_node);

    for (int i = 0; i < num_partition; i++) {
        info->chunkProp[i].resize(kernel_per_node);
        info->chunkEdgeData[i].resize(kernel_per_node);
    }

    for (int p = 0; p < num_partition; p++) {
        log_trace("Allocate mem space in Host side ... partition : %d", p);
        for (int sp = 0; sp < kernel_per_node; sp++) {
#ifndef NOT_USE_GS
            int order_idx = (USE_SCHEDULER == true)? info->order[p][sp] : sp;
            std::string edge_txt_name = directory + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(order_idx) + ".bin";
            std::ifstream edgeFileReader(edge_txt_name, std::ios::binary);
            int edge_num = -1;
            edgeFileReader.read((char*)&edge_num, sizeof(int));
            edgeFileReader.close();
            info->chunkProp[p][sp].edgeNumChunk = edge_num;
            info->chunkEdgeData[p][sp] = new int[edge_num * 2]; // each edge has 2 vertics.
#endif
        }
    }

    for (int sp = 0; sp < kernel_per_node; sp++) {
        info->chunkPropData[sp] = new int[info->alignedCompressedVertexNum];
        info->chunkPropDataNew[sp] = new int[info->alignedCompressedVertexNum];
    }

#if USE_APPLY==true
    info->chunkOutDegData = new int[info->alignedCompressedVertexNum];
    info->chunkOutRegData = new int[info->alignedCompressedVertexNum];
#endif

    log_trace("Allocate mem space in Host side done!");
    return 0;
}


int acceleratorDataPreprocess(const std::string &gName, graphInfo *info)
{
    dataPrepareProperty(info); // function in each application directory

#ifndef NOT_USE_GS
    log_trace("[INFO] load edge txt file after partition ... ");
    // std::string directory = GRAPH_DATASET_DIRETORY + std::to_string(SUB_PARTITION_NUM) + "/";
    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;
    // read edge value
    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < kernel_per_node; sp++) {
            int order_idx = (USE_SCHEDULER == true)? info->order[p][sp] : sp;
            std::string file_name = directory + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(order_idx) + ".bin";
            std::ifstream edgeValueFileReader(file_name, std::ios::binary);
            int read_size = -1;
            edgeValueFileReader.read((char*)&read_size, sizeof(int));
            edgeValueFileReader.read((char*)&read_size, sizeof(int)); // read twice, second value is reserved for future use.
            int* edge_array = new int[read_size * 2];
	        edgeValueFileReader.read((char*)edge_array, read_size * 2 * sizeof(int));
            std::memcpy(info->chunkEdgeData[p][sp], edge_array, read_size * 2 * sizeof(int));
            delete[] edge_array;
            edgeValueFileReader.close();
        }
    }
    log_trace("[INFO] load edge txt file done !");
#endif

    return 0;
}