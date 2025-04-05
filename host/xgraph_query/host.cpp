#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <xcl2.hpp>
#include "CL/cl_ext_xilinx.h"
#include "graph.h"

#include "helper.h"



#include "util/command_parser.h"
#include "log.h"



#define TEST_V (2048)

#define TEST_Q (256)

#define TEST_QD (5)

#define DISTRIBUTION_CHECK (0)


extern CL_API_ENTRY cl_int CL_API_CALL
xclGetMemObjDeviceAddress(cl_mem mem,
                          cl_device_id device,
                          size_t sz,
                          void* address);

//step_update fetch_next

const char * free_run_kernel_list[] = {

};

uint16_t metapath_lut[] = {1, 1, 5, 4, 3, 2, 1, 1}; // label can not be 0

const size_t max_weight_value = 5;




int main(int argc, char** argv) {
    log_info("Start");

    InputParser cmd_parser(argc, argv);
    Graph graph;

    if (cmd_parser.check_cmd_option_exists("-vl")) {
        graph.is_vertex_labeled_ = true;
    }

    if (cmd_parser.check_cmd_option_exists("-el")) {
        graph.is_edge_labeled_ = true;
    }

    if (cmd_parser.check_cmd_option_exists("-vw")) {
        graph.is_vertex_weighted_ = true;
    }

    if (cmd_parser.check_cmd_option_exists("-ew")) {
        graph.is_edge_weighted_ = true;
    }

    graph.is_offset_pair_ = true;


    if (cmd_parser.check_cmd_option_exists("-graph"))
    {

        log_info("%s", cmd_parser.get_cmd_option("-graph").c_str());
        graph.load_csr(cmd_parser.get_cmd_option("-graph"));
    } else
    {
        graph.load_csr("/data/graph_at_scale/youtube/");
        log_info("default graph");
    }


    graph.print_metadata();

    uint32_t vertex_num = graph.num_vertices();


    std::vector<uint32_t> query_array;

    uint32_t query_vertex = 0;
    while (1)
    {
        auto degree = graph.degree(query_vertex);
        if (degree != 0)
        {
            query_array.push_back(query_vertex);
        }
        query_vertex += 1;
        if (query_vertex > vertex_num)
        {
            break;
        }
    }
    shuffle(query_array.data(), query_array.size());


    if ( cmd_parser.check_cmd_option_exists("-query")) {
        uint32_t query_index = std::stoi(cmd_parser.get_cmd_option("-query"));
        int curr_vertex = query_array[query_index];
        printf("query index: [%d]  vertex: %d(0x%x) degree %d  :\n", query_index, curr_vertex, curr_vertex, graph.degree(curr_vertex));
        for (int deg = 0; deg < graph.degree(curr_vertex); deg++)
            printf(" [%d] -> 0x%08x \n", deg, graph.adj_[graph.offset_[curr_vertex] + deg]);
        printf("\n");
    }


    if ( cmd_parser.check_cmd_option_exists("-vertex")) {
        int curr_vertex = std::stoi(cmd_parser.get_cmd_option("-vertex"));
        printf("vertex: %d(0x%x) degree %d  :\n",  curr_vertex, curr_vertex, graph.degree(curr_vertex));
        for (int deg = 0; deg < graph.degree(curr_vertex); deg++)
            printf(" [%d] -> 0x%08x \n", deg, graph.adj_[graph.offset_[curr_vertex] + deg]);
        printf("\n");
    }



#if 0
    for (size_t k = 0; k < vertex_num; k++) {
        while (1)
        {
            uint32_t pv = query_vertex;
            auto degree = csr->rpao[pv + 1] - csr->rpao[pv];
            if (degree != 0)
            {
                break;
            }
            query_vertex += 1;
            query_vertex = query_vertex % vertex_num;
        }
        query_array.push_back(query_vertex);
        query_vertex += 1;
    }
    shuffle(query_array.data(), query_array.size());
#endif
    return 0;
}
