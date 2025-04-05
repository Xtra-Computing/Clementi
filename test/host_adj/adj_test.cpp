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


#include "helper.h"
#include "pcg_basic.h"



const int num_of_data = 256;

const int num_of_curr_data = 128;

const int num_of_last_data = 64;


const char * free_run_kernel_list[] = {
//    "weighted_sampler:{weighted_sampler_1}"
};

cl::Kernel      krnl_free_run[ARRAY_SIZE(free_run_kernel_list)];

int main(int argc, char** argv) {
    std::string binaryFile;
    if (argc < 2) {
        std::string app_name = argv[0];
        app_name = app_name.substr(2, app_name.size() - 6);
        binaryFile = "build_dir_" + app_name + "/kernel.xclbin";
    }
    else
    {
        binaryFile = argv[1];
    }

    pcg32_srandom(42u, 54u);
    printf("starting \n");
    fflush(stdout);


    std::vector<uint32_t, aligned_allocator<uint32_t> > last(num_of_data);
    std::vector<uint32_t, aligned_allocator<uint32_t> > current(num_of_data);
    std::vector<uint32_t, aligned_allocator<uint32_t> > out(num_of_data);

    uint32_t number_of_insert = 20;
#define TEST_INSERT (number_of_insert)

    last[0] = 1;
    current[0] = 1;
    out[0] = 0;
    for (size_t i = 1; i < num_of_data ; i++) {
        last[i] =  last[i - 1] + uint32_t(((double)pcg32_random()) * 10 / ((uint32_t)UINT_MAX));
        ///current[i] = last[i];
        current[i] = current[i - 1] + uint32_t(((double)pcg32_random()) * 10 / ((uint32_t)UINT_MAX));
        out[i] = 0;
    }
#if 0
    last[num_of_data  - 1] =   current[num_of_data  - 1];
    for (size_t i = 0; i < TEST_INSERT ; i++)
    {
        uint32_t insert =  uint32_t(((double)pcg32_random()) * num_of_last_data / ((uint32_t)UINT_MAX)) + 1;
        last[insert] = (last[insert - 1] + last[insert]) / 2;
    }
#endif




    cl_int err;
    cl::CommandQueue q;
    cl::CommandQueue free_run_q;
    cl::Context context;
    cl::Kernel      krnl_test_curr_loader;
    cl::Kernel      krnl_test_last_loader;
    cl::Kernel      krnl_store;

    std::vector<cl::Device> devices = xcl::get_xil_devices();
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    bool valid_device = false;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
        OCL_CHECK(err, q = cl::CommandQueue(context, device,
                                            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE //| CL_QUEUE_PROFILING_ENABLE
                                            , &err));
        OCL_CHECK(err, free_run_q = cl::CommandQueue(context, device,
                                    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE //| CL_QUEUE_PROFILING_ENABLE
                                    , &err));

        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, nullptr, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            std::cout << "Device[" << i << "]: program successful!\n";

            for (unsigned int j = 0; j < ARRAY_SIZE(free_run_kernel_list); j++)
            {
                OCL_CHECK(err, krnl_free_run[j] = cl::Kernel(program, free_run_kernel_list[j], &err));
            }

            OCL_CHECK(err, krnl_test_curr_loader = cl::Kernel(program, "test_load_32:{test_load_32_1}", &err));
#if 0
            OCL_CHECK(err, krnl_test_last_loader = cl::Kernel(program, "test_load_32:{test_load_32_2}", &err));
#else
            OCL_CHECK(err, krnl_test_last_loader = cl::Kernel(program, "test_load_512:{test_load_512_1}", &err));
#endif
            OCL_CHECK(err, krnl_store = cl::Kernel(program, "test_store_32:{test_store_32_1}", &err));

            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }


    OCL_CHECK(err, cl::Buffer     buffer_last(context,
              CL_MEM_USE_HOST_PTR,
              num_of_data * sizeof(uint32_t), last.data(), &err))

    OCL_CHECK(err, cl::Buffer     buffer_curr(context,
              CL_MEM_USE_HOST_PTR,
              num_of_data * sizeof(uint32_t), current.data(), &err))

    OCL_CHECK(err, cl::Buffer     buffer_out(context,
              CL_MEM_USE_HOST_PTR,
              num_of_data * sizeof(uint32_t), out.data(), &err))


    uint32_t number_of_curr_data = num_of_curr_data;
    uint32_t number_of_last_data = num_of_last_data/16;




    OCL_CHECK(err, err = krnl_test_curr_loader.setArg(0, number_of_curr_data));
    OCL_CHECK(err, err = krnl_test_curr_loader.setArg(1, buffer_curr));

    OCL_CHECK(err, err = krnl_test_last_loader.setArg(0, number_of_last_data));
    OCL_CHECK(err, err = krnl_test_last_loader.setArg(1, buffer_last));

    OCL_CHECK(err, err = krnl_store.setArg(0, number_of_curr_data));
    OCL_CHECK(err, err = krnl_store.setArg(1, buffer_out));

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_curr, buffer_last, buffer_out
                                                    }, 0 /*0 means from host*/));

    OCL_CHECK(err, err = q.finish());




    std::vector<double> exec_time;
    for (int i = 0; i < 5; i ++)
    {
        for (size_t j = 0; j < number_of_curr_data ; j++)
        {
            uint32_t ground_truth = 4;
            for (size_t k = 0; k < num_of_last_data; k++)
            {
                if (last[k] == current[j])
                {
                    ground_truth = 2;
                    break;
                }
            }
            printf("[DUMP] @%lu %d %d -> expect: %d\n", j, current[j], last[j], ground_truth);
        }

        for (size_t i = 0; i < num_of_data; i++) {
            out[i] = 0;
        }
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_out
                                                        }, 0 /*0 means from host*/));
        OCL_CHECK(err, err = q.finish());
        double start, stop;

        start = getCurrentTimestamp();
        OCL_CHECK(err, err = q.enqueueTask(krnl_test_curr_loader));
        OCL_CHECK(err, err = q.enqueueTask(krnl_test_last_loader));
        OCL_CHECK(err, err = q.enqueueTask(krnl_store));
        OCL_CHECK(err, err = q.finish());
        stop = getCurrentTimestamp();

        //printf("exec time @%d: %lf\n", i, stop - start);
        exec_time.push_back(stop - start);
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_out}, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = q.finish());

        uint32_t error = 0;
        for (size_t j = 0; j < number_of_curr_data ; j++)
        {
            uint32_t ground_truth  = 4;
            for (size_t k = 0; k < num_of_last_data; k++)
            {
                if (last[k] == current[j])
                {
                    ground_truth = 2;
                    break;
                }
            }
            printf("@%lu %d(0x%x) %d(0x%x) -> %d expect: %d\n", j, current[j],current[j], last[j],last[j], out[j], ground_truth);

            if (out[j] != ground_truth)
            {

                error  ++;
            }
        }
        printf("error: %d\n", error);
    }



    std::sort(exec_time.begin(), exec_time.end());
    printf("[MID] exec time: %lf\n", exec_time[2]);

    return 0;
}

