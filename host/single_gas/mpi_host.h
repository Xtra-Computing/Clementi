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

// Please change this directory into your path.
#define GRAPH_DATASET_DIRETORY  "/data/yufeng/single_4SLR_graph_dataset_bp2/"

void partitionTransfer(graphInfo *info, graphAccelerator *acc);
void setAccKernelArgs(graphInfo *info, graphAccelerator * acc);
int acceleratorInit(graphInfo *info, graphAccelerator* acc);
int acceleratorDataLoad(const std::string &gName, graphInfo *info);
int acceleratorDataPreprocess(const std::string &gName, graphInfo *info);
int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator * acc);
int dataPrepareProperty(graphInfo *info);
int resultTransfer(graphInfo *info, graphAccelerator * acc, int super_step);

#endif /* __MPI_HOST_H__ */ 