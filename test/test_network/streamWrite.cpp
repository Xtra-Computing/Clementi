#include <hls_stream.h>
#include "../gp/fpga_global_mem.h"

extern "C" {
    void  streamWrite(
        uint16                  *vertexPropMem,
        hls::stream<burst_dest>  &vertexPropIn,
        unsigned int            vertexOffset,
        unsigned int            vertexNum
    )
    {
#pragma HLS INTERFACE m_axi port=vertexPropMem offset=slave bundle=gmem0 \
                        max_write_burst_length=64 num_write_outstanding=32

#pragma HLS INTERFACE s_axilite port=vertexPropMem  bundle=control
#pragma HLS INTERFACE s_axilite port=vertexOffset   bundle=control
#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        unsigned int start_idx = vertexOffset >> LOG2_SIZE_BY_INT; // vertexPropMem is 512 bit = 16 * 32bit vertex
        unsigned int burst_num = vertexNum >> (LOG_WRITE_BURST_LEN + LOG2_SIZE_BY_INT); // BURST_LENGTH = 64;
        // Note that vertexNum should be 256 aligned; for example vertexNum = 1024*1024;
        for (int loopCount = 0; loopCount < burst_num; loopCount++) {
            for (int i = 0; i < WRITE_BACK_BURST_LENGTH; i++) {
    #pragma HLS PIPELINE II=1
                burst_dest tmp;
                read_from_stream(vertexPropIn, tmp);
                vertexPropMem[(loopCount << LOG_WRITE_BURST_LEN) + i + start_idx] = tmp.data;
            }
        }
    }
}

