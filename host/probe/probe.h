#ifndef __PROBE_H__
#define __PROBE_H__

#include <malloc.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) sizeof(arr)/sizeof((arr)[0])
#endif



#define MAX_KERNEL_STRING_LENGTH (256)


#define ADD_PROBE_KERNEL()  {                                                                                 \
        char kernel_string[MAX_KERNEL_STRING_LENGTH];                                                   \
        int ns =  snprintf(kernel_string, ARRAY_SIZE(kernel_string),"test_probe_%d:{test_probe_%d_%d}", \
                        this->width,this->width, this->cu_index);                                       \
        if (ns > MAX_KERNEL_STRING_LENGTH){ printf("Overflow error in add kernel"); exit(-1); }         \
        OCL_CHECK(err, this->krnl = cl::Kernel(this->program, kernel_string, &err));                    \
}

#define CU_ID_MAP(id)   (id + 1)

class probe
{
private:
    cl::Device device;
    cl::Context context;
    cl::Program program;

    cl::CommandQueue q;

    cl::CommandQueue res_q;

    cl::Kernel     krnl;

    cl_mem            probe_mem;
    cl::Buffer        probe_buffer;
    int cu_index;
    int width;
    bool buffer_alloced = false;

    void * p_data;
    int probe_size;
    int total_size;
    cl_int err;


public:
    double start_time, stop_time;

    probe()
    {
    }

    probe(cl::Device device, cl::Context context, cl::Program program, int index, int width)
    {
        this->init(device, context, program, index, width);
    }

    int init(cl::Device device, cl::Context context, cl::Program program, int index, int width)
    {
#if ENABLE_PROBE
        this->device = device;
        OCL_CHECK(err, this->context = cl::Context(this->device, nullptr, nullptr, nullptr, &err));
        this->program = program;
        this->cu_index = CU_ID_MAP(index);
        this->width = width;
        OCL_CHECK(err, this->q = cl::CommandQueue(this->context, this ->device,
                                 CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE //| CL_QUEUE_PROFILING_ENABLE
                                 , &err));
        OCL_CHECK(err, this->res_q = cl::CommandQueue(this->context, this ->device,
                                 CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE //| CL_QUEUE_PROFILING_ENABLE
                                 , &err));
        ADD_PROBE_KERNEL();
#endif

        return 0;
    }



    int set_probe_depth(int size)
    {
#if ENABLE_PROBE
        probe_size = size;
        total_size = width / 8 * size;
        p_data = memalign(64 * 1024, total_size);
        memset(p_data, 0 , total_size);
        buffer_alloced = true;
        OCL_CHECK(err, probe_mem =  clCreateBuffer(this->context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, this->total_size, p_data, &err));
        probe_buffer = cl::Buffer(probe_mem);

        OCL_CHECK(err, err = this->krnl.setArg(0, this->probe_size));
        OCL_CHECK(err, err = this->krnl.setArg(1, this->probe_buffer));
        OCL_CHECK(err, err = this->q.enqueueMigrateMemObjects({probe_buffer}, 0 /*0 means from host*/));
        OCL_CHECK(err, err = this->q.finish());
#endif
        return 0;
    }



    void* get_res(void)
    {
#if ENABLE_PROBE
        OCL_CHECK(err, err = this->res_q.enqueueMigrateMemObjects({this->probe_buffer}, CL_MIGRATE_MEM_OBJECT_HOST));
        OCL_CHECK(err, err = this->res_q.finish());
        int number_per_line = this->width / 8;
        for (int i = 0; i < this->probe_size; i ++)
        {
            printf("probe @%d: ", i);
            for (int j = 0; j < this->width / 8; j ++)
            {
                printf("%02x", ((uint8_t*)(this->p_data))[i * number_per_line  + (number_per_line - 1 - j)]);
            }
            printf("\n");
        }
        return this->p_data;
#else
        return nullptr;
#endif
    }

    int exec(void)
    {
        this->start();
        this->stop();
        return 0;
    }
    int start(void)
    {
#if ENABLE_PROBE
        OCL_CHECK(err, err = this->q.enqueueTask(this->krnl));
#endif
        return 0;
    }
    int stop(void)
    {
#if ENABLE_PROBE
        OCL_CHECK(err, err = this->q.finish());
#endif
        return 0;
    }

    ~probe()
    {
        if (buffer_alloced)
        {
            free(this->p_data);
            buffer_alloced = false;
        }
    }

};


#endif /* __PROBE_H__*/

