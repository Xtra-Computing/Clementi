#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"


#define N_BIS (512)

extern "C" {
    void test_load_512(     int              size,
                            ap_uint<N_BIS>    *mem,
                            hls::stream< ap_axiu< N_BIS, 0, 0, 0> >  &output,
                            ctrl_stream_t      &ctrl

                           ) {
        test_mem_loader_end_ctrl< hls::stream< ap_axiu< N_BIS, 0, 0, 0> >,  ap_uint<N_BIS>, ap_axiu< N_BIS, 0, 0, 0>>(size, mem, output, ctrl);
    }
}
