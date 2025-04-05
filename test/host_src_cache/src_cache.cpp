#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <xcl2.hpp>

#include "unistd.h"
#include "limits.h"
#include "rw_data_struct.h"
#include "gp_data_struct.h"

#include "helper.h"
#include "pcg_basic.h"

#include "host_test_common.h"

#define LOG_CACHE_LINE_SIZE (8)
#define CACHE_LINE_SIZE (1<<LOG_CACHE_LINE_SIZE)

const int num_of_data = 16 * 128;
const int num_of_src_prop = 1024 * 1024 * 2;


const char * free_run_kernel_list[] = {
//    "weighted_sampler:{weighted_sampler_1}"
};

cl::Kernel      krnl_free_run[ARRAY_SIZE(free_run_kernel_list)];

int main(int argc, char** argv) {

    std::vector<uint32_t, aligned_allocator<uint32_t> > src_prop(num_of_src_prop);

    for (int i = 0; i < num_of_src_prop; i++)
    {
#if 0
        src_prop[i] = pcg32_random();
#else
        src_prop[i] = pcg32_random() % 16;
#endif
    }

    std::vector<uint32_t, aligned_allocator<uint32_t> > src_vertex(num_of_data);

#if 1
    src_vertex[0] = uint32_t(pcg32_random() % CACHE_LINE_SIZE );
    for (size_t i = 1; i < num_of_data ; i++) {
        src_vertex[i] =  src_vertex[i - 1] + uint32_t(pcg32_random() % 2 );
    }
#else

    for (size_t i = 0; i < num_of_data ; i++) {
        src_vertex[i] =  i;
    }
#endif


    std::vector<uint32_t, aligned_allocator<uint32_t> > out_result(num_of_data);
    for (int i = 0; i < num_of_data; i ++)
    {
        out_result[i] = 0;
    }

    cl::Kernel      krnl_load_512;
    cl::Kernel      krnl_store_32;
    cl::Kernel      krnl_src_cache;
    //cl::Kernel      krnl_src_cache_pe;

    HOST_INIT(
        OCL_CHECK(err, krnl_load_512  = cl::Kernel(program, "test_load_512:{test_load_512_1}", &err));
        OCL_CHECK(err, krnl_store_32  = cl::Kernel(program, "test_store_512:{test_store_512_1}", &err));
        OCL_CHECK(err, krnl_src_cache = cl::Kernel(program, "src_cache_read_engine:{src_cache_read_engine_1}", &err));
        //OCL_CHECK(err, krnl_src_cache_pe = cl::Kernel(program, "src_cache_access:{src_cache_access_1}", &err));
    );

    HOST_BUFFER(src_prop, num_of_src_prop * sizeof(uint32_t));
    HOST_BUFFER(src_vertex,   num_of_data * sizeof(uint32_t));
    HOST_BUFFER(out_result,   num_of_data * sizeof(uint32_t));


    uint32_t num_of_src_vertex_load = num_of_data / 16;

    uint32_t num_of_src_vertex_store = num_of_data / 16 ;



    OCL_CHECK(err, err = krnl_load_512.setArg(0, num_of_src_vertex_load));
    OCL_CHECK(err, err = krnl_load_512.setArg(1, buffer_src_vertex));

    OCL_CHECK(err, err = krnl_store_32.setArg(0, num_of_src_vertex_store));
    OCL_CHECK(err, err = krnl_store_32.setArg(1, buffer_out_result));

    OCL_CHECK(err, err = krnl_src_cache.setArg(0, buffer_src_prop));


    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_src_prop, buffer_src_vertex, buffer_out_result
                                                    }, 0 /*0 means from host*/));
    OCL_CHECK(err, err = q.finish());

    std::vector<double> exec_time;
    for (int i = 0; i < 2; i ++)
    {
        double start, stop;

        start = getCurrentTimestamp();
        OCL_CHECK(err, err = q.enqueueTask(krnl_load_512));
        OCL_CHECK(err, err = q.enqueueTask(krnl_store_32));
        OCL_CHECK(err, err = q.enqueueTask(krnl_src_cache));
        //OCL_CHECK(err, err = q.enqueueTask(krnl_src_cache_pe));
        OCL_CHECK(err, err = q.finish());
        stop = getCurrentTimestamp();

        //printf("exec time @%d: %lf\n", i, stop - start);
        exec_time.push_back(stop - start);
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_out_result}, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = q.finish());

#if 0
        uint32_t reduced_result[16];
        for (size_t j = 0; j < 16; j++)
        {
            reduced_result[j] = 0;
        }
        for (size_t j = 0; j < num_of_src_vertex_store * 16; j++)
        {
            reduced_result[j % 16] +=  src_prop[src_vertex[j] % num_of_src_prop];
        }
        for (size_t j = 0; j < 16; j++)
        {
            uint32_t res  = out_result[j];
            printf("[%ld] res is %u groundtruth %u \n", j, res, reduced_result[j]);
        }
#else
        uint32_t error_count = 0;
        for (size_t j = 0; j < num_of_src_vertex_store * 16; j++)
        {
            uint32_t res  = out_result[j];
            if (res != (src_prop[src_vertex[j] % num_of_src_prop]))
            {
                error_count ++;
                printf("[%ld] res is %u groundtruth %u \n", j, res, src_prop[src_vertex[j] % num_of_src_prop]);
            }

        }
        printf("%d errors\n", error_count);
#endif


    }


    return 0;
}

