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


uint32_t get_msb(uint32_t n)
{
    if (n == 0)
        return 0;

    int msb = 0;
    n = n / 2;
    while (n != 0) {
        n = n / 2;
        msb++;
    }

    return (msb + 1);
}

#define SIZE_OF_BURST (2)


const int num_of_data = 128;

const int max_weight = 256;

uint32_t weight_table[] = {
    1,1,0,1,
    0,1,0,1,
    0,4,0,1,
    0,1,0,16,0
};

uint32_t dist_res[num_of_data * 16];

const char * free_run_kernel_list[] = {
//    "weighted_sampler:{weighted_sampler_1}"
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


    std::vector<burst_sync_padded_t, aligned_allocator<burst_sync_padded_t> > burst_sync(num_of_data);
    std::vector<weight_t, aligned_allocator<weight_t> > weight(num_of_data);
    std::vector<weight_t, aligned_allocator<weight_t> > normalized_weight(num_of_data);
    std::vector<weight_t, aligned_allocator<weight_t> > rngs(num_of_data);
    std::vector<uint32_t, aligned_allocator<uint32_t> > res(num_of_data);
    std::vector<uint32_t, aligned_allocator<uint32_t> > golden_res(num_of_data);
    std::vector<uint32_t, aligned_allocator<uint32_t> > rs_mask(num_of_data);

    std::vector<uint32_t, aligned_allocator<uint32_t> > tps(num_of_data);

    std::vector<scaled_weight_t, aligned_allocator<scaled_weight_t> > scaled(num_of_data);
    std::vector<weight_t, aligned_allocator<weight_t> > trunced(num_of_data);

    uint32_t msb = 0;
    uint32_t temporary_perfix_sum = 0;
    for (size_t i = 0; i < num_of_data; i++) {
        burst_sync_padded_t sync;
        sync.addr =  i * 16;
        sync.id  = 0;

        if (i % SIZE_OF_BURST == SIZE_OF_BURST - 1)
        {
            sync.burst_id = i | 0x8000;
        }
        else
        {
            sync.burst_id = i;
        }

        sync.dynamic_id = 0;
        weight_t w;
        weight_t rng;
        scaled_weight_t sw;
        weight_t tw;

        uint32_t  mask = 0;
        for (size_t j = 0 ; j < 16; j ++)
        {
            //w.data[j]   = (pcg32_random() + 1) % max_weight;
            //w.data[j]   = ((32 - j * 2) + 1) % max_weight;
            w.data[j] = weight_table[j + 1];
            temporary_perfix_sum += w.data[j];
            rng.data[j] = pcg32_random();
            sw.data[j] = (uint64_t)(temporary_perfix_sum) * rng.data[j];
            tw.data[j] = sw.data[j] >> 32;
            if (tw.data[j] < w.data[j])
            {
                mask |= (0x1 << j);
            }
        }
        if (get_msb(mask) != 0)
        {
            msb = get_msb(mask) + sync.addr;
        }
        rs_mask[i] = mask;
        burst_sync[i] = sync;
        weight[i] = w;
        rngs[i] = rng;
        res[i] = 0;
        if (i % SIZE_OF_BURST == SIZE_OF_BURST - 1)
        {
            golden_res[i / SIZE_OF_BURST] = msb;
            tps[i / SIZE_OF_BURST] = temporary_perfix_sum;
            temporary_perfix_sum = 0;
        }
        if (i == (num_of_data - 1))
        {
            for (size_t j = 0 ; j < 16; j ++)
            {
                printf("0x%04x \n", w.data[j]);
            }
            printf("burst id 0x%04x \n", sync.burst_id);
        }
    }

    for (size_t i = 0; i < num_of_data; i++) {
        uint32_t tps_data = tps[i / SIZE_OF_BURST];
        weight_t w;
        weight_t w_ori = weight[i];
        for (size_t j = 0 ; j < 16; j ++)
        {
            w.data[j] = w_ori.data[j] * 100000 / tps_data;
        }
        normalized_weight[i] = w;
    }




    cl_int err;
    cl::CommandQueue q;
    cl::CommandQueue free_run_q;
    cl::Context context;
    cl::Kernel      krnl_test_bs_loader;
    cl::Kernel      krnl_test_weight_loader;
    cl::Kernel      krnl_test_rng_loader;
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

            OCL_CHECK(err, krnl_test_bs_loader = cl::Kernel(program, "test_bs_loader:{test_bs_loader_1}", &err));
            //OCL_CHECK(err, krnl_test_rng_loader = cl::Kernel(program, "test_weight_loader:{test_weight_loader_2}", &err));

            OCL_CHECK(err, krnl_test_weight_loader = cl::Kernel(program, "test_weight_loader:{test_weight_loader_1}", &err));
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
              num_of_data * sizeof(burst_sync_padded_t), burst_sync.data(), &err));
    OCL_CHECK(err, cl::Buffer     buffer_weight(context,
              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
              num_of_data * sizeof(weight_t), weight.data(), &err))

    OCL_CHECK(err, cl::Buffer     buffer_rngs(context,
              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
              num_of_data * sizeof(weight_t), rngs.data(), &err))

    OCL_CHECK(err, cl::Buffer        buffer_res(context,
              CL_MEM_USE_HOST_PTR,
              num_of_data * sizeof(uint32_t), res.data(), &err));

    uint32_t number_of_query = num_of_data;
    uint32_t max_number_of_store = num_of_data;





    OCL_CHECK(err, err = krnl_test_bs_loader.setArg(0, number_of_query));
    OCL_CHECK(err, err = krnl_test_bs_loader.setArg(1, buffer_burst_sync));

    OCL_CHECK(err, err = krnl_test_weight_loader.setArg(0, number_of_query));
    OCL_CHECK(err, err = krnl_test_weight_loader.setArg(1, buffer_weight));

    //OCL_CHECK(err, err = krnl_test_rng_loader.setArg(0, number_of_query));
    //OCL_CHECK(err, err = krnl_test_rng_loader.setArg(1, buffer_rngs));


    OCL_CHECK(err, err = krnl_store.setArg(0, max_number_of_store));
    OCL_CHECK(err, err = krnl_store.setArg(1, buffer_res));

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_burst_sync, buffer_weight, buffer_res, buffer_rngs
                                                    }, 0 /*0 means from host*/));

    OCL_CHECK(err, err = q.finish());


    for (unsigned int j = 0; j < ARRAY_SIZE(free_run_kernel_list); j++)
    {
        OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_free_run[j]));
    }


    std::vector<double> exec_time;
    for (int i = 0; i < 10000; i ++)
    {
        for (size_t i = 0; i < num_of_data; i++) {
            res[i] = 0;
        }
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_res
                                                        }, 0 /*0 means from host*/));
        OCL_CHECK(err, err = q.finish());
        double start, stop;
        start = getCurrentTimestamp();
        OCL_CHECK(err, err = q.enqueueTask(krnl_store));
        OCL_CHECK(err, err = q.enqueueTask(krnl_test_bs_loader));
        OCL_CHECK(err, err = q.enqueueTask(krnl_test_weight_loader));
        //OCL_CHECK(err, err = q.enqueueTask(krnl_test_rng_loader));

        //OCL_CHECK(err, err = q.enqueueTask(krnl_sampler));
        OCL_CHECK(err, err = q.finish());
        stop = getCurrentTimestamp();

        //printf("exec time @%d: %lf\n", i, stop - start);
        exec_time.push_back(stop - start);
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_res}, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = q.finish());

        for (auto j = 0; j < num_of_data / SIZE_OF_BURST; j++)
        {
            if (j < 10)
            {
                //printf("@%d %d \n", i, res[j]);
            }
            if (res[j] > num_of_data * 16 )
            {
                printf("[ERROR] @%d @%d hw %d \n",i, j, res[j]);
            }
            dist_res[res[j]] ++;
        }
    }

    for (int  i = 0; i < 2 ; i ++)
    {
        weight_t w = normalized_weight[i];
        for (int j = 0; j < 16 ; j ++)
        {
            printf("[DUMP] @%d w %lf d %d \n", i * 16 + j, w.data[j] / 100000.0, dist_res[i * 16 + j]);
        }
    }

    std::sort(exec_time.begin(), exec_time.end());
    printf("[MID] exec time: %lf\n", exec_time[2]);

    return 0;
}

