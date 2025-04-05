#include <hls_stream.h>
#include "../graph_fpga.h"

extern "C" {
    void streamSync(
        hls::stream<burst_dest>&  input,  // Internal Stream
        hls::stream<burst_dest>&  output, // Internal Stream
        int  dest,
        int  FIFO_length, // can be configured by host
        int  vertexNum
        ) {
#pragma HLS INTERFACE s_axilite port=dest           bundle=control
#pragma HLS INTERFACE s_axilite port=FIFO_length    bundle=control
#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control
sync:
    int loopNum = (vertexNum >> 4);
    int read_count = 0;
    int write_count = 0;
    burst_dest merge_vertex;
    burst_dest forward_vertex;
    while ((write_count < loopNum) || (read_count < loopNum)) {

        // output to merge kernel
        if (((write_count - read_count) < FIFO_length) && (write_count < loopNum)) {
            merge_vertex.data = 0;
            merge_vertex.dest = dest;
            merge_vertex.keep = -1;
            if (((loopNum - 1) == write_count) || (((write_count + 1) % 22) == 0))
                merge_vertex.last = 1;
            else
                merge_vertex.last = 0;
            output.write(merge_vertex);
            write_count ++;
        }

        if ((!input.empty()) && (read_count < loopNum)) {
            input.read(forward_vertex);
            read_count ++;
        }

    }

}
}