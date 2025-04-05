#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"



#define BURST_BUFFER_SIZE (32)
const int max_burst_length = BURST_BUFFER_SIZE;

extern "C" {
    void test_vertex_loader(int                         size,
                            vertex_descriptor_item_t    *vertex,
                            vertex_descriptor_stream_t  &output

                           ) {
#pragma HLS INTERFACE m_axi port=vertex offset=slave bundle=gmem num_read_outstanding=2 max_read_burst_length=max_burst_length

        vertex_descriptor_item_t buffer[BURST_BUFFER_SIZE];

        for (int i = 0; i < size; i += BURST_BUFFER_SIZE) {

            for (int j = 0; j < BURST_BUFFER_SIZE; j++)
            {
                buffer[j] = vertex[i + j];
            }

            for (int j = 0; j < BURST_BUFFER_SIZE; j++)
            {
                vertex_descriptor_pkg_t v_pkg;
                v_pkg.data = buffer[j];
                v_pkg.last = ((i + j) == (size - 1));
                output.write(v_pkg);
            }
        }
    }
}
