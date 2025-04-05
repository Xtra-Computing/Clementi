#include <hls_stream.h>
#include "../header/fpga_global_mem.h"

void perfCounterProc(hls::stream<int>& cmd_start, hls::stream<int>& cmd_end, int *out) {
    int cnt = 0;
    int val = cmd_start.read(); // read a start signal
count:
    while (cmd_end.read_nb(val) == false) {
        // wait to receive a value to end counting
        cnt++;
    }
    out[0] = cnt; // write back the cycle number.
}

void writeToArray ( uint16 *vertexPropMem, hls::stream<burst_dest> &vertexPropIn, 
                    unsigned int vertexOffset, unsigned int vertexNum, hls::stream<int> &perfEndStrm) {
    unsigned int start_idx = vertexOffset >> LOG2_SIZE_BY_INT;
    unsigned int burst_num = vertexNum >> (LOG_WRITE_BURST_LEN + LOG2_SIZE_BY_INT); // BURST_LENGTH = 64;
    for (int loopCount = 0; loopCount < burst_num; loopCount++) {
        for (int i = 0; i < WRITE_BACK_BURST_LENGTH; i++) {
#pragma HLS PIPELINE II=1
            burst_dest tmp;
            read_from_stream(vertexPropIn, tmp);
            vertexPropMem[(loopCount << LOG_WRITE_BURST_LEN) + i + start_idx] = tmp.data;
        }
    }
    perfEndStrm.write(0); // end flag
}

extern "C" {
    void  streamWrite(
        uint16                      *vertexPropMem,
        hls::stream<burst_dest>     &vertexPropIn,
        unsigned int                vertexOffset,
        unsigned int                vertexNum,
        hls::stream<int>            &perfStartStrm,
        int                         *perfCount
    )
    {
#pragma HLS INTERFACE m_axi port=vertexPropMem offset=slave bundle=gmem0 \
                        max_write_burst_length=64 num_write_outstanding=32
#pragma HLS INTERFACE m_axi port=perfCount offset=slave bundle=gmem1 \
                        max_write_burst_length=1 num_write_outstanding=1

#pragma HLS INTERFACE s_axilite port=vertexPropMem bundle=control
#pragma HLS INTERFACE s_axilite port=perfCount bundle=control
#pragma HLS INTERFACE s_axilite port=vertexOffset   bundle=control
#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        hls::stream<int>      perfEndStrm;
#pragma HLS DATAFLOW
        writeToArray(vertexPropMem, vertexPropIn, vertexOffset, vertexNum, perfEndStrm);
        perfCounterProc(perfStartStrm, perfEndStrm, perfCount);
    }
}

