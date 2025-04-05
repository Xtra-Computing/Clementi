#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"


#define N_BIS (512)

extern "C" {
    void test_duplicate_512(hls::stream< ap_axiu< N_BIS, 0, 0, 0> >  &in,
                            hls::stream< ap_axiu< N_BIS, 0, 0, 0> >  &out1,
                            hls::stream< ap_axiu< N_BIS, 0, 0, 0> >  &out2)
    {
#pragma HLS interface ap_ctrl_none port = return
        while (true)
        {
            ap_axiu< N_BIS, 0, 0, 0> pkg;
            if (in.read_nb(pkg))
            {
                out1.write(pkg);
                out2.write(pkg);
            }
        }
    }
}
