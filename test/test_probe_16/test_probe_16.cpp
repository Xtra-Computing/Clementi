#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "test_common.h"

#define BIT_WIDTH   (16)

extern "C" {
    void test_probe_16   (  int                                            size,
                            ap_uint<BIT_WIDTH>                             *mem,
                            hls::stream< ap_axiu< BIT_WIDTH, 0, 0, 0> >    &input,
                            hls::stream< ap_axiu< BIT_WIDTH, 0, 0, 0> >    &output

                      ) {

        probe_data<BIT_WIDTH>(size, mem, input, output);
    }
}
