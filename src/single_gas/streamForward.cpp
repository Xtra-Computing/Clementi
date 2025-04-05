#include <hls_stream.h>
#include "../graph_fpga.h"
#include "../header/fpga_global_mem.h"

extern "C" {
    void  streamForward(
        hls::stream<burst_dest>  &input, // the upstream maybe apply kernel or network kernel.
        hls::stream<burst_dest>  &output_a,
        hls::stream<burst_dest>  &output_b
    ) { // freerun kernel 
#pragma HLS interface ap_ctrl_none port=return
        int loopNum = (MAX_VERTICES_IN_ONE_PARTITION >> 4); // for each partition exec;
        while (1) { // freerun
            for (int i = 0; i < loopNum; i++) {
#pragma HLS PIPELINE II=1
                burst_dest  unit;
                read_from_stream(input, unit);
                unit.dest =  1;
                unit.keep = -1;

                // if (((loopNum - 1) == i) || ((((i + 1)*64)% 1408) == 0))
                if (((loopNum - 1) == i) || (((i + 1) % 22) == 0))
                    unit.last = 1;
                else
                    unit.last = 0;

                write_to_stream(output_a, unit);
                write_to_stream(output_b, unit);
            }
        }
    }
}