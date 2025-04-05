#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"


#define N_BIS (512)

extern "C" {
    void test_padding_512(  hls::stream< ap_axiu< N_BIS, 0, 0, 0> >  &input,
                            hls::stream< ap_axiu< N_BIS, 0, 0, 0> >  &output
                         ) {
#pragma HLS interface ap_ctrl_none port=return
        while (true) {
#pragma HLS PIPELINE II=1
            ap_axiu< N_BIS, 0, 0, 0>  pkg = input.read();
            output.write(pkg);
        }
    }
}
