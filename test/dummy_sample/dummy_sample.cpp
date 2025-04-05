#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "stream_operation.h"


#define FLUSH_WINDOW  8

extern "C" {
    void dummy_sample (     burst_sync_stream_t             &burst_sync,
                            weight_stream_t                 &weight_input,
                            next_vertex_ordered_stream_t    &update
                      ) {
#pragma HLS interface ap_ctrl_none port=return

free_run: while (1)
        {
            volatile uint8_t flush_counter = 0;
read: while (1)
            {
                burst_sync_pkg_t  sync_pkg;
                if (burst_sync.read_nb(sync_pkg)) {
                    flush_counter = 0;
                    weight_pkg_t weight_pkg = weight_input.read();

                    if (sync_pkg.data.ap_member(burst_sync_t, burst_id) & 0x8000)
                    {
                        next_vertex_ordered_pkg_t out_pkg;
                        out_pkg.data.ap_member(next_vertex_ordered_t, vertex) = weight_pkg.data.range(31, 0);
                        out_pkg.data.ap_member(next_vertex_ordered_t, id)
                            = sync_pkg.data.ap_member(burst_sync_t, id);
                        out_pkg.last = sync_pkg.last;
                        update.write(out_pkg);
                    }
                }
                else
                {
                    flush_counter ++;
                    if (flush_counter > FLUSH_WINDOW)
                    {
                        break;
                    }
                }
            }
        }
    }
}
