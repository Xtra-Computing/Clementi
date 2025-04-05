#include <hls_stream.h>
#include <ap_int.h>
#include "../header/fpga_global_mem.h"
#include "../graph_fpga.h"

extern "C" {
    void  readProp(
        uint16                  *vertexPropMem,
        hls::stream<burst_dest> &vertexTrans,
        unsigned int            node_id,
        unsigned int            dest_id,
        unsigned int            vertexOffset,
        unsigned int            vertexNum
    )
    {
#pragma HLS INTERFACE m_axi port=vertexPropMem offset=slave bundle=gmem0 \
                        max_read_burst_length=64 num_read_outstanding=32

#pragma HLS INTERFACE s_axilite port=vertexPropMem  bundle=control
#pragma HLS INTERFACE s_axilite port=node_id        bundle=control
#pragma HLS INTERFACE s_axilite port=dest_id        bundle=control
#pragma HLS INTERFACE s_axilite port=vertexOffset   bundle=control
#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        int loop_num = ((vertexNum - 1) >> 4) + 1;
        int packet_num = (loop_num + 21 - 1) / 21; // packet size = 1408 bytes = 22*512 bit; first frame reserved;
        int start_idx = vertexOffset >> 4; // SIZE_BY_INT = 4; 512bit = 16 * 32bit int type.

        burst_dest out_raw;
        int address = 0;
        int frameId = 0;

        for (int i = 0; i < (loop_num + packet_num); i++) {
#pragma HLS PIPELINE II=1
            if (frameId == 0) { // first frame
                out_raw.data.range(31, 0) = node_id;
                out_raw.data.range(63, 32) = start_idx + address;
            } else { // valid data
                out_raw.data = vertexPropMem[start_idx + address];
                address ++;
            }

            if ((frameId == 21) || ((i+1) == (loop_num + packet_num))) {
                out_raw.last = 1;
                frameId = 0;
            } else {
                out_raw.last = 0;
                frameId ++;
            }

            out_raw.dest = dest_id;
            out_raw.keep = -1;
            vertexTrans << out_raw;
        }

        return;
    }
}


