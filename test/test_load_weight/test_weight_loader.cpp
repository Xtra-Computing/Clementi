#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"




extern "C" {
    void test_weight_loader(int              size,
                            weight_item_t    *mem,
                            weight_stream_t  &output

                           ) {
        test_mem_loader<weight_stream_t, weight_item_t, weight_pkg_t>(size, mem, output);
    }
}
