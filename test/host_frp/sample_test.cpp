#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <xcl2.hpp>

#include "unistd.h"
#include "rw_data_struct.h"

#include "helper.h"
#include "pcg_basic.h"

#define TEST_SIZE (1)




const char * free_run_kernel_list[] = {

};

cl::Kernel      krnl_free_run[ARRAY_SIZE(free_run_kernel_list)];

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string binaryFile = argv[1];

    pcg32_srandom(42u, 54u);
    printf("starting \n");
    fflush(stdout);

    const int num_of_data = 1024;

    const int max_weight = 256;
    std::vector<burst_sync_t, aligned_allocator<burst_sync_t> > burst_sync(num_of_data);
    std::vector<weight_t, aligned_allocator<weight_t> > weight(num_of_data);
    std::vector<uint32_t, aligned_allocator<uint32_t> > res(num_of_data);
    for (size_t i = 0; i < num_of_data; i++) {
        burst_sync_t sync;
        sync.id  = i;
        sync.burst_id = i | 0x8000;
        sync.dynamic_id = i;
        weight_t w;
        for (size_t j = 0 ; j < 16; j ++)
        {
            w.data[j] =  i;
        }
        burst_sync[i] = sync;
        weight[i] = w;
        res[i] = 0;
    }


    cl_int err;
    cl::CommandQueue q;
    cl::CommandQueue free_run_q;
    cl::Context context;
    cl::Kernel      krnl_test_bs_loader;
    cl::Kernel      krnl_test_weight_loader;
    cl::Kernel      krnl_sampler;
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

            //OCL_CHECK(err, krnl_test_bs_loader = cl::Kernel(program, "test_bs_loader:{test_bs_loader_1}", &err));
            OCL_CHECK(err, krnl_test_weight_loader = cl::Kernel(program, "test_weight_loader:{test_weight_loader_1}", &err));
            OCL_CHECK(err, krnl_sampler = cl::Kernel(program, "weighted_sampler:{weighted_sampler_1}", &err));
            OCL_CHECK(err, krnl_store = cl::Kernel(program, "test_store_32:{test_store_32_1}", &err));

            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }

    OCL_CHECK(err, cl::Buffer buffer_burst_sync(context,
              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
              num_of_data * sizeof(burst_sync_t), burst_sync.data(), &err));
    OCL_CHECK(err, cl::Buffer     buffer_weight(context,
              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
              num_of_data * sizeof(weight_t), weight.data(), &err))

    OCL_CHECK(err, cl::Buffer        buffer_res(context,
              CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
              num_of_data * sizeof(uint32_t), res.data(), &err));

    uint32_t number_of_query = num_of_data;
    uint32_t max_number_of_store = num_of_data;


    OCL_CHECK(err, err = krnl_test_weight_loader.setArg(0, number_of_query));
    OCL_CHECK(err, err = krnl_test_weight_loader.setArg(1, buffer_weight));

    OCL_CHECK(err, err = krnl_store.setArg(0, max_number_of_store));

    OCL_CHECK(err, err = krnl_store.setArg(1, buffer_res));

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_burst_sync, buffer_weight, buffer_res
                                                    }, 0 /*0 means from host*/));

    OCL_CHECK(err, err = q.finish());


    for (unsigned int j = 0; j < ARRAY_SIZE(free_run_kernel_list); j++)
    {
        OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_free_run[j]));
    }


    std::vector<double> exec_time;
    for (int i = 0; i < 5; i ++)
    {
        double start, stop;
        start = getCurrentTimestamp();
        OCL_CHECK(err, err = q.enqueueTask(krnl_store));
        //OCL_CHECK(err, err = q.enqueueTask(krnl_test_bs_loader));
        OCL_CHECK(err, err = q.enqueueTask(krnl_test_weight_loader));
        //OCL_CHECK(err, err = q.enqueueTask(krnl_sampler));

        stop = getCurrentTimestamp();
        printf("enqueue  time: %lf\n", stop - start);

        OCL_CHECK(err, err = q.finish());
        stop = getCurrentTimestamp();

        printf("exec time: %lf\n", stop - start);
        exec_time.push_back(stop - start);
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_res}, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = q.finish());
    }
    std::sort(exec_time.begin(), exec_time.end());
    printf("[MID] exec time: %lf\n", exec_time[2]);

    return 0;
}

