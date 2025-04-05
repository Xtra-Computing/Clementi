#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "test_common.h"

#define BIT_WIDTH   (16)

extern "C" {
    void test_store_16  (  int                                            size,
                        ap_uint<BIT_WIDTH>                             *mem,
                        hls::stream< ap_axiu< BIT_WIDTH, 0, 0, 0> >    &input

                      ) {
        test_store<BIT_WIDTH>(size, mem, input);
    }
}
