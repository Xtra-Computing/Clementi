#include <hls_stream.h>
#include "../graph_fpga.h"
#include "../header/fpga_global_mem.h"
#include "../header/fpga_apply.h"

extern "C" {
    void  streamMerge(
        hls::stream<burst_dest>  &input_a,
        hls::stream<burst_dest>  &input_b,
        hls::stream<burst_dest>  &output
    ) { // freerun kernel
#pragma HLS interface ap_ctrl_none port=return
        int loopNum = (MAX_VERTICES_IN_ONE_PARTITION >> 4);  // for each partition exec;
        while(1) {
            for (int i = 0; i < loopNum; i++) {
#pragma HLS PIPELINE II=1
                burst_dest  unit[2];
            #pragma HLS ARRAY_PARTITION variable=unit dim=0 complete

                read_from_stream(input_a, unit[0]);
                read_from_stream(input_b, unit[1]);

                burst_dest res;
                for (int inner = 0; inner < 16 ; inner ++)
                {
            #pragma HLS UNROLL
                    res.data.range(31 + inner * 32, 0 + inner * 32) 
                        = PROP_COMPUTE_STAGE4 ( unit[0].data.range(31 + inner * 32, 0 + inner * 32),
                                                unit[1].data.range(31 + inner * 32, 0 + inner * 32) );
                }

                // if (((loopNum - 1) == i) || ((((i + 1)*64)% 1408) == 0))
                if (((loopNum - 1) == i) || (((i + 1) % 22) == 0))
                    res.last = 1;
                else
                    res.last = 0;
                res.dest =  1;
                res.keep = -1;
                write_to_stream(output, res);
            }
        }
    }
}