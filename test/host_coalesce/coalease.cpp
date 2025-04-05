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

#define LOG_CACHE_LINE_SIZE (8)
#define CACHE_LINE_SIZE (1<<LOG_CACHE_LINE_SIZE)

const int num_of_data = 256;


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

    std::vector<uint32_t, aligned_allocator<uint32_t> > source_vertex(num_of_data);
    std::vector<cache_command_t, aligned_allocator<cache_command_t> > cmd(num_of_data);

    uint32_t number_of_cmd = 0;
    std::vector<uint32_t> groundtruth_cmd;

    source_vertex[0] = 1;
    uint32_t last_cmd = source_vertex[0]/ CACHE_LINE_SIZE;
    groundtruth_cmd.push_back(last_cmd);
    number_of_cmd ++;
    for (size_t i = 1; i < num_of_data ; i++) {
        source_vertex[i] =  source_vertex[i - 1] + uint32_t(pcg32_random() % CACHE_LINE_SIZE );
        uint32_t current_cmd = source_vertex[i]/CACHE_LINE_SIZE;
        if (current_cmd != last_cmd) {
            groundtruth_cmd.push_back(current_cmd);
            number_of_cmd++;
        }
        last_cmd = current_cmd;
    }


    cl::Kernel      krnl_vertex_loader;
    cl::Kernel      krnl_cmd_store;


    cl_int err;
    cl::CommandQueue q;
    cl::CommandQueue free_run_q;
    cl::Context context;

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

            OCL_CHECK(err, krnl_vertex_loader = cl::Kernel(program, "test_load_512:{test_load_512_1}", &err));

            OCL_CHECK(err, krnl_cmd_store = cl::Kernel(program, "test_store_64:{test_store_64_1}", &err));

            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }

    OCL_CHECK(err, cl::Buffer     buffer_source_vertex(context,
              CL_MEM_USE_HOST_PTR,
              num_of_data * sizeof(uint32_t), source_vertex.data(), &err))

    OCL_CHECK(err, cl::Buffer     buffer_cmd(context,
              CL_MEM_USE_HOST_PTR,
              num_of_data * sizeof(cache_command_t), cmd.data(), &err))


    uint32_t number_of_source_vertex = num_of_data/16;




    OCL_CHECK(err, err = krnl_vertex_loader.setArg(0, number_of_source_vertex));
    OCL_CHECK(err, err = krnl_vertex_loader.setArg(1, buffer_source_vertex));

    OCL_CHECK(err, err = krnl_cmd_store.setArg(0, number_of_cmd));
    OCL_CHECK(err, err = krnl_cmd_store.setArg(1, buffer_cmd));

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_source_vertex, buffer_cmd
                                                    }, 0 /*0 means from host*/));

    OCL_CHECK(err, err = q.finish());




    std::vector<double> exec_time;
    for (int i = 0; i < 2; i ++)
    {
        double start, stop;

        start = getCurrentTimestamp();
        OCL_CHECK(err, err = q.enqueueTask(krnl_vertex_loader));
        OCL_CHECK(err, err = q.enqueueTask(krnl_cmd_store));
        OCL_CHECK(err, err = q.finish());
        stop = getCurrentTimestamp();

        //printf("exec time @%d: %lf\n", i, stop - start);
        exec_time.push_back(stop - start);
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_cmd}, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = q.finish());

        printf("number of command %d\n",number_of_cmd);
        for (size_t j = 0; j < number_of_cmd ; j++)
        {
            cache_command_t single_cmd = cmd[j];
            printf("cmd %d  is %d groundtruth %d \n", j, single_cmd.idx, groundtruth_cmd[j]);

        }
    }


    return 0;
}

