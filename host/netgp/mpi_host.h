#ifndef __MPI_HOST_H__
#define __MPI_HOST_H__

#include <string>
#include <vector>

#ifdef CXX_MPI
#include <mpi.h>
#endif

#include "mpi_graph_api.h"
#include "../log/log.h"

#define ENDFLAG                 0xffffffff

#if PROCESSOR_NUM == 4
    #define GRAPH_DATASET_DIRETORY  "/data/yufeng/4node_4SLR_graph_dataset_bp2/"
#endif
#if PROCESSOR_NUM == 3
    #define GRAPH_DATASET_DIRETORY  "/data/yufeng/3node_4SLR_graph_dataset/"
#endif
#if PROCESSOR_NUM == 2
    #define GRAPH_DATASET_DIRETORY  "/data/yufeng/2node_4SLR_graph_dataset_bp2/"
#endif
#if PROCESSOR_NUM == 1
    #define GRAPH_DATASET_DIRETORY  "/data/yufeng/single_4SLR_graph_dataset_bp2/"
#endif

void partitionTransfer(graphInfo *info, graphAccelerator *acc);
void setAccKernelArgs(graphInfo *info, graphAccelerator * acc);
int acceleratorInit(graphInfo *info, graphAccelerator* acc);
int acceleratorDataLoad(const std::string &gName, graphInfo *info);
int acceleratorDataPreprocess(const std::string &gName, graphInfo *info);
int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator * acc);
int dataPrepareProperty(graphInfo *info);
int resultTransfer(graphInfo *info, graphAccelerator * acc, int super_step);

#endif /* __MPI_HOST_H__ */ 