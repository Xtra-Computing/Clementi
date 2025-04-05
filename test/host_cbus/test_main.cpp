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
#include "cbus_data_struct.h"



uint32_t select_address(uint32_t self, uint32_t* addr_table)
{
    while (1)
    {
        uint32_t rng =  (pcg32_random()) % 4;
        if ((rng != self) && (addr_table[rng] != 0xff))
        {
            addr_table[rng] = 0xff;
            //printf("%d \n", rng);
            return rng;
        }
    }
}

uint32_t select_address_without_self_check(uint32_t* addr_table)
{
    while (1)
    {
        uint32_t rng =  (pcg32_random()) % 4;
        if ( (addr_table[rng] != 0xff))
        {
            addr_table[rng] = 0xff;
            //printf("%d \n", rng);
            return rng;
        }
    }
}

const char * free_run_kernel_list[] = {

};

cl::Kernel      krnl_free_run[ARRAY_SIZE(free_run_kernel_list)];

#if 1
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string binaryFile = argv[1];

    pcg32_srandom(42u, 54u);
    printf("starting \n");
    fflush(stdout);

    const int num_of_data = 16384;

    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > buffer_1(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > buffer_2(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > buffer_3(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > buffer_4(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > res_1(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > res_2(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > res_3(num_of_data);
    std::vector<cbus_raw_t, aligned_allocator<cbus_raw_t> > res_4(num_of_data);

    for (size_t i = 0; i < num_of_data; i++) {
        uint32_t id_array[4] = {0, 1, 2, 3};
        uint32_t addr;
        uint32_t sufflue_id[4];
        sufflue_id[0] = select_address(0, id_array);
        sufflue_id[1] = select_address(1, id_array);
        sufflue_id[2] = select_address_without_self_check(id_array);
        sufflue_id[3] = select_address_without_self_check(id_array);

        if ((sufflue_id[2] == 2) || (sufflue_id[3] == 3))
        {
            uint32_t tmp = sufflue_id[2];
            sufflue_id[2] = sufflue_id[3];
            sufflue_id[3] = tmp;
        }



        addr = sufflue_id[0];
        cbus_raw_t input_1;
        input_1.dest = (i & 0x3fffffff ) | (addr << 30);
        input_1.status = addr;
        buffer_1[i] = input_1;

        addr = sufflue_id[1];
        cbus_raw_t input_2;
        input_2.dest = (i & 0x3fffffff ) | (addr << 30);
        input_2.status = addr;
        buffer_2[i] = input_2;

        addr = sufflue_id[2];
        cbus_raw_t input_3;
        input_3.dest = (i & 0x3fffffff ) | (addr << 30);
        input_3.status = addr;
        buffer_3[i] = input_3;

        addr = sufflue_id[3];
        cbus_raw_t input_4;
        input_4.dest = (i & 0x3fffffff ) | (addr << 30);
        input_4.status = addr;
        buffer_4[i] = input_4;


    }

    for (size_t i = 0; i < num_of_data; i++) {
        cbus_raw_t res;
        res.dest    = 0;
        res.status  = 0;
        res_1[i] = res;
        res_2[i] = res;
        res_3[i] = res;
        res_4[i] = res;
    }


    cl_int err;
    cl::CommandQueue q;
    cl::CommandQueue free_run_q;
    cl::Context context;

    cl::Kernel      krnl_load_1;
    cl::Kernel      krnl_load_2;
    cl::Kernel      krnl_load_3;
    cl::Kernel      krnl_load_4;
    cl::Kernel      krnl_store_1;
    cl::Kernel      krnl_store_2;
    cl::Kernel      krnl_store_3;
    cl::Kernel      krnl_store_4;

    cl::Kernel      krnl_cbus_1;
    cl::Kernel      krnl_cbus_2;
    cl::Kernel      krnl_cbus_3;
    cl::Kernel      krnl_cbus_4;

    std::vector<cl::Device> devices = xcl::get_xil_devices();
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    bool valid_device = false;
    cl::Device device;
    for (unsigned int i = 0; i < devices.size(); i++) {
        device = devices[i];
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

            OCL_CHECK(err, krnl_store_1 = cl::Kernel(program, "test_store_64:{test_store_64_1}", &err));
            OCL_CHECK(err, krnl_load_1  = cl::Kernel(program, "test_load_64:{test_load_64_1}", &err));
            OCL_CHECK(err, krnl_cbus_1 = cl::Kernel(program, "cbus_interface_0:{cbus_interface_0_1}", &err));

            OCL_CHECK(err, krnl_store_2 = cl::Kernel(program, "test_store_64:{test_store_64_2}", &err));
            OCL_CHECK(err, krnl_load_2  = cl::Kernel(program, "test_load_64:{test_load_64_2}", &err));
            OCL_CHECK(err, krnl_cbus_2 = cl::Kernel(program, "cbus_interface_1:{cbus_interface_1_1}", &err));

            OCL_CHECK(err, krnl_store_3 = cl::Kernel(program, "test_store_64:{test_store_64_3}", &err));
            OCL_CHECK(err, krnl_load_3  = cl::Kernel(program, "test_load_64:{test_load_64_3}", &err));
            OCL_CHECK(err, krnl_cbus_3 = cl::Kernel(program, "cbus_interface_2:{cbus_interface_2_1}", &err));

            OCL_CHECK(err, krnl_store_4 = cl::Kernel(program, "test_store_64:{test_store_64_4}", &err));
            OCL_CHECK(err, krnl_load_4  = cl::Kernel(program, "test_load_64:{test_load_64_4}", &err));
            OCL_CHECK(err, krnl_cbus_4 = cl::Kernel(program, "cbus_interface_3:{cbus_interface_3_1}", &err));

            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }

    OCL_CHECK(err, cl::Buffer buffer_in_1(context,
                                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          num_of_data * sizeof(cbus_raw_t), buffer_1.data(), &err));
    OCL_CHECK(err, cl::Buffer  buffer_res_1(context,
                                            CL_MEM_USE_HOST_PTR,
                                            num_of_data * sizeof(cbus_raw_t), res_1.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_in_2(context,
                                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          num_of_data * sizeof(cbus_raw_t), buffer_2.data(), &err));
    OCL_CHECK(err, cl::Buffer  buffer_res_2(context,
                                            CL_MEM_USE_HOST_PTR,
                                            num_of_data * sizeof(cbus_raw_t), res_2.data(), &err));


    OCL_CHECK(err, cl::Buffer buffer_in_3(context,
                                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          num_of_data * sizeof(cbus_raw_t), buffer_3.data(), &err));
    OCL_CHECK(err, cl::Buffer  buffer_res_3(context,
                                            CL_MEM_USE_HOST_PTR,
                                            num_of_data * sizeof(cbus_raw_t), res_3.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_in_4(context,
                                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          num_of_data * sizeof(cbus_raw_t), buffer_4.data(), &err));
    OCL_CHECK(err, cl::Buffer  buffer_res_4(context,
                                            CL_MEM_USE_HOST_PTR,
                                            num_of_data * sizeof(cbus_raw_t), res_4.data(), &err));


    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({   buffer_in_1, buffer_in_2, buffer_in_3, buffer_in_4,
                         buffer_res_1, buffer_res_2, buffer_res_3, buffer_res_4
                                                    }, 0 /*0 means from host*/));

    OCL_CHECK(err, err = q.finish());



    OCL_CHECK(err, err = krnl_store_1.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_store_1.setArg(1, buffer_res_1));
    OCL_CHECK(err, err = krnl_load_1.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_load_1.setArg(1, buffer_in_1));

    OCL_CHECK(err, err = krnl_store_2.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_store_2.setArg(1, buffer_res_2));
    OCL_CHECK(err, err = krnl_load_2.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_load_2.setArg(1, buffer_in_2));


    OCL_CHECK(err, err = krnl_store_3.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_store_3.setArg(1, buffer_res_3));
    OCL_CHECK(err, err = krnl_load_3.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_load_3.setArg(1, buffer_in_3));


    OCL_CHECK(err, err = krnl_store_4.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_store_4.setArg(1, buffer_res_4));
    OCL_CHECK(err, err = krnl_load_4.setArg(0, num_of_data));
    OCL_CHECK(err, err = krnl_load_4.setArg(1, buffer_in_4));


    for (unsigned int j = 0; j < ARRAY_SIZE(free_run_kernel_list); j++)
    {
        OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_free_run[j]));
    }
    OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_cbus_1));
    OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_cbus_2));
    OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_cbus_3));
    OCL_CHECK(err, err = free_run_q.enqueueTask(krnl_cbus_4));

    std::vector<double> exec_time;
    //for (int i = 0; i < 5; i ++)
    {
        double start, stop;
        start = getCurrentTimestamp();


        OCL_CHECK(err, err = q.enqueueTask(krnl_store_1));
        OCL_CHECK(err, err = q.enqueueTask(krnl_store_2));
        OCL_CHECK(err, err = q.enqueueTask(krnl_store_3));
        OCL_CHECK(err, err = q.enqueueTask(krnl_store_4));

        OCL_CHECK(err, err = q.enqueueTask(krnl_load_1));
        OCL_CHECK(err, err = q.enqueueTask(krnl_load_2));
        OCL_CHECK(err, err = q.enqueueTask(krnl_load_3));
        OCL_CHECK(err, err = q.enqueueTask(krnl_load_4));

        stop = getCurrentTimestamp();
        printf("enqueue  time: %lf\n", stop - start);

        OCL_CHECK(err, err = q.finish());
        stop = getCurrentTimestamp();

        printf("exec time: %lf\n", stop - start);
        exec_time.push_back(stop - start);
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({
            buffer_res_1, buffer_res_2,
            buffer_res_3, buffer_res_4
        }, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = q.finish());
    }

    for (int i = 0; i < 20; i ++)
    {
        printf("original 1 @ %d %u %u\n", i, buffer_1[i].dest, buffer_1[i].status);
        printf("         2 @ %d %u %u\n", i, buffer_2[i].dest, buffer_2[i].status);
        printf("         3 @ %d %u %u\n", i, buffer_3[i].dest, buffer_3[i].status);
        printf("         4 @ %d %u %u\n", i, buffer_4[i].dest, buffer_4[i].status);
    }
    for (int i = 0; i < 20; i ++)
    {
        printf("buffer 1 @ %d %u %u\n", i, res_1[i].dest, res_1[i].status);
        printf("       2 @ %d %u %u\n", i, res_2[i].dest, res_2[i].status);
        printf("       3 @ %d %u %u\n", i, res_3[i].dest, res_3[i].status);
        printf("       4 @ %d %u %u\n", i, res_4[i].dest, res_4[i].status);
    }
    //std::sort(exec_time.begin(), exec_time.end());
    //printf("[MID] exec time: %lf\n", exec_time[2]);

    return 0;
}

#else
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
#endif