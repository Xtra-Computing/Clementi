// Host code implementation for single FPGA based GAS graph processing.
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

    graphInfo graphDataInfo;
    graphAccelerator thunderGraph;
    std::string g_name = parser.value("dataset");

    acceleratorDataLoad(g_name, &graphDataInfo);
    log_info("[INFO] graph load done");

    acceleratorInit(&graphDataInfo, &thunderGraph);
    log_info("[INFO] acc init done");

    acceleratorDataPreprocess(g_name, &graphDataInfo); // data prepare + data partition
    log_info("[INFO] Processor graph preprocess done");
    // acceleratorCModelDataPreprocess(&graphDataInfo); // cmodel data preprocess, for verification

    partitionTransfer(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor partitionTransfer done");

    setAccKernelArgs(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor setAccKernelArgs done");


    // run super steps
    int super_step = std::stoi(parser.value("super_step"));
    log_info("[INFO] Processor run super step = %d", super_step);

    int elapsed_time = 0;
    for (int s = 0; s < super_step; s++) {

        auto start_kernel_superstep = chrono::steady_clock::now();

        acceleratorExecute (s, &graphDataInfo, &thunderGraph);

        auto end_kernel_superstep = chrono::steady_clock::now();
        elapsed_time = chrono::duration_cast<chrono::microseconds>(end_kernel_superstep - start_kernel_superstep).count();
        std::cout << "elapsed_time = " << elapsed_time << " ms" << std::endl;
    }

    // need to add result transfer function
    resultTransfer(&graphDataInfo, &thunderGraph, super_step);
}
