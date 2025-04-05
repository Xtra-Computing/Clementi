#include "../cmdparser/cmdlineparser.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <cstdlib>

#include "mpi_graph_api.h"
#include "mpi_host.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

int main(int argc, char** argv) {

    sda::utils::CmdLineParser parser;
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.addSwitch("--super_step", "-s", "super step number", "5"); // set super-step number as 5
    parser.parse(argc, argv);

    log_set_level(LOG_INFO); // set log level LOG_INFO;

    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    log_debug("MPI init done: %d / %d ", world_rank, world_size);

    if (world_size != PROCESSOR_NUM) {
        std::cout << "world_size = "  << world_size << " PROCESS_NUM = " << PROCESSOR_NUM << std::endl;
        log_error("MPI number does not equal to setting !");
        return 0;
    }

    graphInfo graphDataInfo;
    graphAccelerator thunderGraph;
    std::string g_name = parser.value("dataset");

    log_info("[INFO] Processor %d start ... loading graph %s", world_rank, g_name.c_str());

    acceleratorDataLoad(g_name, &graphDataInfo);
    log_info("[INFO] Processor %d graph load done", world_rank);

    acceleratorInit(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor %d acc init done", world_rank);

    acceleratorDataPreprocess(g_name, &graphDataInfo); // data prepare + data partition
    log_info("[INFO] Processor %d graph preprocess done", world_rank);
    // acceleratorCModelDataPreprocess(&graphDataInfo); // cmodel data preprocess, for verification

    partitionTransfer(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor %d partitionTransfer done", world_rank);

    setAccKernelArgs(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor %d setAccKernelArgs done", world_rank);

    MPI_Barrier(MPI_COMM_WORLD); // sync barrier;


    // run super steps
    int super_step = std::stoi(parser.value("super_step"));
    log_info("[INFO] Processor %d run super step = %d", world_rank, super_step);

    std::vector<std::vector<int>> time_array;
    time_array.resize(super_step);
    for (int i = 0; i < super_step; i++) {
        time_array[i].resize(world_size);
    }


    for (int s = 0; s < super_step; s++) {

        auto start_kernel_superstep = chrono::steady_clock::now();

        acceleratorExecute (s, &graphDataInfo, &thunderGraph);

        auto end_kernel_superstep = chrono::steady_clock::now();
        int elapsed_time = chrono::duration_cast<chrono::microseconds>(end_kernel_superstep - start_kernel_superstep).count();

        MPI_Barrier(MPI_COMM_WORLD); // get each node execution time;
        MPI_Gather(&elapsed_time, 1, MPI_INT, time_array[s].data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD); // sync barrier ensure each super step execute independently.
    }

    log_info("[INFO] Processor %d run super step done", world_rank);

    MPI_Barrier(MPI_COMM_WORLD);
    // report time result.
    if (world_rank == 0) {
        std::cout << "Hardware execution time: " << std::endl;
        for (int i = 0; i < super_step; i++) {
            std::cout << "super_step[" << i << "] ";
            for (int j = 0; j < world_size; j++) {
                std::cout << time_array[i][j] << " ";
            }
            std::cout << "us" << std::endl;
        }
    }

    // need to add result transfer function
    resultTransfer(&graphDataInfo, &thunderGraph, super_step);

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
