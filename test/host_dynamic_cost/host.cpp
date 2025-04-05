#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <xcl2.hpp>

#include "graph.h"
#include "rw_data_struct.h"

#include "helper.h"
#include "pcg_basic.h"
#include "profile.h"


const char * free_run_kernel_list[] = {

};




#ifndef SIZE_ALIGNMENT
inline long unsigned int get_aligned_size(unsigned int in, unsigned int align)
{
    if (in == 0)
    {
        return align;
    }
    else
    {
        return (((((in - 1) / align) + 1) * align));
    }
}
#define SIZE_ALIGNMENT(in,align)    get_aligned_size((unsigned int)in,(unsigned int)align)
#endif


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }
    //printf("test %d %d\n",RANGE_TEST(step_metadata_t, curr_vertex));
    //exit(0);

    std::string binaryFile = argv[1];
    Graph *graph =  createGraph(argv[2], "normal");
    CSR* csr    = new CSR(*graph);
    //csr->save2File("graph.csr");
    free(graph);

    pcg32_srandom(42u, 54u);

    uint32_t vertex_num = csr->vertexNum;
    uint32_t hist[65];

    for (size_t i = 0; i < 65; i++)
    {
        hist[i] = 0;
    }


    for (size_t i = 0; i < vertex_num; i++) {
        vertex_descriptor_t v;
        v.start = csr->rpao[i];
        v.size  = csr->rpao[i + 1] - csr->rpao[i];
        uint32_t aligned = SIZE_ALIGNMENT(v.size , 16) / 16;
        if (v.size <= 1)
        {
            hist[0]++;
        }
        else if (aligned > 64)
        {
            hist[64] ++;
        }
        else
        {
            hist[aligned] ++;
        }

    }
    uint64_t total_size[65];
    total_size[0] = hist[0];
    for (size_t i = 1; i < 65; i++)
    {
        total_size[i] =  hist[i] * 64 * i;
    }
    total_size[0] = hist[0];
    uint64_t edges = total_size[0];
    for (size_t i = 1; i < 65; i++)
    {
        edges += total_size[i];
        printf("dist@%2ld  %d size %ld \n", i, hist[i], total_size[i]);
    }
    printf("edges :%ld \n", edges);
    double  total_time[ARRAY_SIZE(title)];
    for (size_t i = 0; i < ARRAY_SIZE(title); i++)
    {
        total_time[i] = 0;
        for (size_t j = 0; j < 65; j++)
        {
            double bandwidth = dynamic_bd[j * ARRAY_SIZE(title) + i];
            //printf("bd%lf\n",bandwidth);
            double time = total_size[j] / bandwidth /1000.0/1000.0;
            total_time[i] += time;
        }
        printf("[RES] %s bandwidth is %lf step %lf \n",
            title[i],
            edges/total_time[i]/1000.0/1000.0,
            vertex_num/total_time[i]/1000.0/1000.0);
    }







    return 0;
}

