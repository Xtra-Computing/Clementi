#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "stream_operation.h"



extern "C" {
        void burst_cmd_sink(burst_cmd_stream_t  &input) {
#pragma HLS interface ap_ctrl_none port = return
                while (1) {
#pragma HLS PIPELINE II=1
                        burst_cmd_pkg_t  data;
                        read_from_stream(input, data);
                }
        }
}
