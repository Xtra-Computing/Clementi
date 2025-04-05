#ifndef __MPI_PREPROCESS_H__
#define __MPI_PREPROCESS_H__

#include <cmath>
#include <cstdlib>

#include "mpi_host.h"

#define INT2FLOAT                   (std::pow(2,30))
#define kDamp               			(0.85f)
#define MAX_PROP                    (0x7fffffff - 1)

int float2int(float a) {
    return (int)(a * INT2FLOAT);
}

float int2float(int a) {
    return ((float)a / INT2FLOAT);
}

unsigned int dataPrepareGetArg(graphInfo *info)
{
    return float2int((1.0f - kDamp) / info->vertexNum);
}

// default data prepare function for pagerank;
int dataPrepareProperty(graphInfo *info)
{
    float init_score_float = 1.0f / (info->vertexNum);
    int init_score_int = float2int(init_score_float);

    for (int sp = 0; sp < SUB_PARTITION_NUM / PROCESSOR_NUM; sp++) {
        for (int i = 0; i < info->alignedCompressedVertexNum; i++) {
            if (i >= info->compressedVertexNum) {
                info->chunkPropData[sp][i] = 0;
                info->chunkOutDegData[i] = 0;
                info->chunkPropDataNew[sp][i] = 0;
            } else {
                info->chunkOutDegData[i] = info->outDeg[i];
                info->chunkPropData[sp][i] = init_score_int / info->outDeg[i];
                info->chunkPropDataNew[sp][i] = 0;
            }
        }
    }
    return 0;
}

int dataPrepareProperty_wcc(graphInfo *info)
{
    for (int sp = 0; sp < SUB_PARTITION_NUM / PROCESSOR_NUM; sp++) {
        for (int i = 0; i < info->alignedCompressedVertexNum; i++) {
            info->chunkPropData[sp][i] = 0;
        }
        int select_index  = ((double)std::rand()) / ((RAND_MAX + 1u) / info->compressedVertexNum);
        info->chunkPropData[sp][select_index] = 1;
    }
    return 0;
}

#endif /* __MPI_PREPROCESS_H__ */ 