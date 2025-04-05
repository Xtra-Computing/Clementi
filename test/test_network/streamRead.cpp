#include <hls_stream.h>
#include "../../src/gp/fpga_global_mem.h"
#include "../../src/graph_fpga.h"

extern "C" {
    void  streamRead(
        uint16                  *vertexPropMem,
        hls::stream<burst_dest>  &vertexPropStrm,
        unsigned int            vertexOffset,
        unsigned int            vertexNum
    )
    {
#pragma HLS INTERFACE m_axi port=vertexPropMem offset=slave bundle=gmem0 \
                        max_read_burst_length=64 num_read_outstanding=32

#pragma HLS INTERFACE s_axilite port=vertexPropMem  bundle=control
#pragma HLS INTERFACE s_axilite port=vertexOffset   bundle=control
#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        unsigned int burst_read_size = 64;
        unsigned int burst_num = ((vertexNum - 1) >> (4 + 6)) + 1; // LOG2_SIZE_BY_INT = 4; + 6; burst len;
        unsigned int start_idx = vertexOffset >> 4; // SIZE_BY_INT = 4; 512bit = 16 * 32bit int type.

        for (unsigned int i = 0; i < burst_num; i++) {
            for (unsigned int j = 0; j < burst_read_size; j++) {
    #pragma HLS PIPELINE II=1
                burst_dest out_raw;
                out_raw.data = vertexPropMem[j + i * burst_read_size + start_idx];
                vertexPropStrm << out_raw;
            }
        }
        return;
    }
}


