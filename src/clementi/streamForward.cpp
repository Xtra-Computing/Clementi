#include <hls_stream.h>
#include "../graph_fpga.h"
#include "../header/fpga_global_mem.h"

extern "C" {
    void  streamForward(
        hls::stream<burst_dest>  &input,
        hls::stream<burst_dest>  &output_a,
        hls::stream<burst_dest>  &output_b
    ) { // freerun kernel 
#pragma HLS interface ap_ctrl_none port=return
        while (1) {
            burst_dest  unit;
            read_from_stream(input, unit);
            write_to_stream(output_a, unit);
            write_to_stream(output_b, unit);
        }
    }
}