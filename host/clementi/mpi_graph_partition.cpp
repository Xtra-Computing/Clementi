
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
    for (int p = 0; p < 200; p++) {
        for (int sp = 0; sp < 20; sp ++) {
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

int acceleratorSchedule(const std::string &gName, graphInfo *info)
{
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    if (world_rank < 0) log_error("world rank error !");
    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    std::ifstream inputFile(directory + gName + "/schedule_" + std::to_string(world_size) + ".txt");

    std::vector<std::vector<int>> intArrays;
    if (!inputFile.is_open()) {
        std::cout << "Failed to open the file." << std::endl;
        return 1;
    } else {
        std::string line;
        while (std::getline(inputFile, line)) {
            if (line.find("PartitionList") != std::string::npos) { // header of each line
                std::vector<int> intArray;
                std::istringstream iss(line);
                std::string nodeStr;
                int num;
                iss >> nodeStr; // skip the string item
                while (iss >> num) {
                    intArray.push_back(num);
                }
                intArrays.push_back(intArray);
            }
        }
    }
    inputFile.close();
    std::cout << "World rank " << world_rank << " : ";
    info->scheduleItem.clear();
    int p_size = intArrays[world_rank].size();
    for (int i = 0; i < p_size; ++i) {
        info->scheduleItem.push_back(intArrays[world_rank][i]);
        std::cout << intArrays[world_rank][i] << " " ;
    }
    std::cout << std::endl;
    log_info("[INFO] Processor %d schedule done", world_rank);
    return 0;
}

int acceleratorDataLoad(const std::string &gName, graphInfo *info)
{
    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    log_debug("Load graph data from directory : %s", directory.c_str());

    int num_partition = getPartitionNum(directory + gName, KERNEL_NUM);
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


    // Allocate edge mem space in Host side
    info->compressedVertexNum = num_mapped;
    int aligned_vertex_num = ((num_mapped + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
    info->alignedCompressedVertexNum = aligned_vertex_num;
    log_debug("whole graph compressed vertex num %d, algned %d", num_mapped, aligned_vertex_num);


    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");
    int kernel_per_node = KERNEL_NUM;
    int schedule_num = info->scheduleItem.size();

    info->chunkProp.resize(schedule_num); // scheduleItem size graph
    info->chunkEdgeData.resize(schedule_num);
    info->chunkPropData.resize(kernel_per_node);
    info->chunkPropDataNew.resize(kernel_per_node);

    for (int i = 0; i < schedule_num; i++) {
        info->chunkProp[i].resize(kernel_per_node);
        info->chunkEdgeData[i].resize(kernel_per_node);
    }

    for (int p = 0; p < schedule_num; p++) {
        log_trace("Allocate mem space in Host side ... partition : %d", p);
        for (int sp = 0; sp < kernel_per_node; sp++) {
            int schedule_idx = info->scheduleItem[p];
            std::string edge_txt_name = directory + gName + "/p_" + std::to_string(schedule_idx) + "_sp_" + std::to_string(sp) + ".bin";
            std::ifstream edgeFileReader(edge_txt_name, std::ios::binary);
            int edge_num = -1;
            edgeFileReader.read((char*)&edge_num, sizeof(int));
            edgeFileReader.close();
            info->chunkProp[p][sp].edgeNumChunk = edge_num;
            info->chunkEdgeData[p][sp] = new int[edge_num * 2]; // each edge has 2 vertics.
        }
    }

    for (int sp = 0; sp < kernel_per_node; sp++) {
        info->chunkPropData[sp] = new int[info->alignedCompressedVertexNum];
        info->chunkPropDataNew[sp] = new int[info->alignedCompressedVertexNum];
    }

#if USE_APPLY==true
    info->chunkOutDegData = new int[info->alignedCompressedVertexNum];
    info->chunkOutRegData = new int[info->alignedCompressedVertexNum];
    info->propTemp = new int[info->alignedCompressedVertexNum];
#endif

    log_trace("Allocate mem space in Host side done!");
    return 0;
}


int acceleratorDataPreprocess(const std::string &gName, graphInfo *info)
{
#if TYPE==pr
    dataPrepareProperty(info); // function in each application directory
#elif ((TYPE==wcc)||(TYPE==rw))
    dataPrepareProperty_wcc_rw(info); // function in each application directory
#endif

    log_trace("[INFO] load edge txt file after partition ... ");
    std::string directory = std::string(GRAPH_DATASET_DIRETORY);
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");
    int schedule_num = info->scheduleItem.size();
    // read edge value
    for (int p = 0; p < schedule_num; p++) {
        for (int sp = 0; sp < KERNEL_NUM; sp++) {
            int schedule_idx = info->scheduleItem[p];
            std::string file_name = directory + gName + "/p_" + std::to_string(schedule_idx) + "_sp_" + std::to_string(sp) + ".bin";
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

    return 0;
}