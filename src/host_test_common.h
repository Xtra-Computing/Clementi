#ifndef __HOST_TEST_COMMON_H__
#define __HOST_TEST_COMMON_H__



#define HOST_BUFFER(name, size)  OCL_CHECK(err, cl::Buffer     buffer_##name(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, size, name.data(), &err));

#define HOST_INIT(__CODE__) std::string binaryFile; \
if (argc < 2) { \
    std::string app_name = argv[0]; \
    app_name = app_name.substr(2, app_name.size() - 6); \
    binaryFile = "build_dir_" + app_name + "/kernel.xclbin"; \
} \
else { binaryFile = argv[1]; } \
pcg32_srandom(42u, 54u); \
printf("starting \n"); \
fflush(stdout); \
cl_int err; \
cl::CommandQueue q; \
cl::CommandQueue free_run_q; \
cl::Context context; \
std::vector<cl::Device> devices = xcl::get_xil_devices(); \
auto fileBuf = xcl::read_binary_file(binaryFile); \
cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}}; \
bool valid_device = false; \
for (unsigned int i = 0; i < devices.size(); i++) { \
    auto device = devices[i]; \
    OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err)); \
    OCL_CHECK(err, q = cl::CommandQueue(context, device, \
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE \
                                        , &err)); \
    OCL_CHECK(err, free_run_q = cl::CommandQueue(context, device, \
                                CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE \
                                , &err)); \
    std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl; \
    cl::Program program(context, {device}, bins, nullptr, &err); \
    if (err != CL_SUCCESS) { \
        std::cout << "Failed to program device[" << i << "] with xclbin file!\n"; \
    } else { \
        std::cout << "Device[" << i << "]: program successful!\n"; \
        for (unsigned int j = 0; j < ARRAY_SIZE(free_run_kernel_list); j++) \
        { \
            OCL_CHECK(err, krnl_free_run[j] = cl::Kernel(program, free_run_kernel_list[j], &err)); \
        } \
        { __CODE__ }; \
        valid_device = true; \
        break; \
    } \
} \
if (!valid_device) { \
    std::cout << "Failed to program any device found, exit!\n"; \
    exit(EXIT_FAILURE); \
}

#endif /* __HOST_TEST_COMMON_H__ */
