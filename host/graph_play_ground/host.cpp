#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <xcl2.hpp>
#include "CL/cl_ext_xilinx.h"
#include "graph.h"

#include "helper.h"


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
    if (argc < 2) {
        std::cout << "Usage: " << argv[0]  << "<Graph>" << std::endl;
        return EXIT_FAILURE;
    }
    printf("starting \n");
    fflush(stdout);
    //printf("test %d %d\n",RANGE_TEST(step_metadata_t, curr_vertex));
    //exit(0);

    Graph *graph =  createGraph(argv[1], "normal");
    CSR* csr    = new CSR(*graph);
    //csr->save2File("graph.csr");
    free(graph);
    std::vector<uint32_t> query_array;
    uint32_t pv = TEST_V;

    if (argc>2)
        pv = atoi(argv[2]);

    auto degree = csr->rpao[pv + 1] - csr->rpao[pv];
    printf("%d(0x%x) degreee %d  :\n", pv,pv, degree);
    for (uint32_t i =0; i <  degree; i++)
    {
        printf("\t v%d is %d(0x%x) \n",i, csr->ciao[csr->rpao[pv] + i], csr->ciao[csr->rpao[pv] + i]);
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
