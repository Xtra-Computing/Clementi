#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"


#define BIT_WIDTH   (512)

#define UNIT_SIZE   (128)
extern "C" {
    void step_source(   int                        size,
                        dummy_step_item_t          *mem,
                        step_metadata_stream_t     &out

                    ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem

        for (int i = 0; i < size; i++)
        {
            step_metadata_pkg_t pkg;
            dummy_step_item_t item = mem[i];

            if (i == (size - 1))
                pkg.last = 1;
            else
                pkg.last = 0;
            pkg.data.ap_member(step_metadata_t, curr_vertex) = item.ap_member(dummy_step_t, curr_vertex);
            pkg.data.ap_member(step_metadata_t, remain_step) = item.ap_member(dummy_step_t, remain_step);
            pkg.data.ap_member(step_metadata_t, state)       = item.ap_member(dummy_step_t, state);
            pkg.data.ap_member(step_metadata_t, ring_state)  = item.ap_member(dummy_step_t, ring_state);
            pkg.data.ap_member(step_metadata_t, inner_state) = item.ap_member(dummy_step_t, inner_state);
            pkg.data.ap_member(step_metadata_t, assert)      = item.ap_member(dummy_step_t, assert);
            out.write(pkg);
        }
    }
}
