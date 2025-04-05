#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"


#define BIT_WIDTH   (512)

#define UNIT_SIZE   (128)
extern "C" {
    void step_sink (    int                        size,
                        dummy_step_item_t          *mem,
                        step_metadata_stream_t     &in

                     ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_write_outstanding=64

        for (int i =0; i < size; i++)
        {
            step_metadata_pkg_t pkg = in.read();
            mem[i] = pkg.data;

        }
    }
}
