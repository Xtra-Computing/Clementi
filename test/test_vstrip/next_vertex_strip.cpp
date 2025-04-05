#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "stream_operation.h"



extern "C" {
        void next_vertex_strip( next_vertex_ordered_stream_t            &in,
                                next_vertex_stream_t                    &out) {
#pragma HLS interface ap_ctrl_none port = return
                while (1) {
#pragma HLS PIPELINE II=1 style=frp
                        next_vertex_ordered_pkg_t  in_pkg;
                        in_pkg = in.read();
                        next_vertex_pkg_t out_pkg;
                        out_pkg.data = in_pkg.data.ap_member(next_vertex_ordered_t, vertex);
                        out_pkg.last = in_pkg.last;
                        out.write(out_pkg);
                }
        }
}
