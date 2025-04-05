#include "latency_common.h"

extern "C" {
    void time_diff( diff_time_stream_t &tag_start,
                    diff_time_stream_t &tag_end,
                    diff_time_stream_t &time_out)
    {
#pragma HLS interface ap_ctrl_none port=return
        volatile uint8_t state  = 0u;

        volatile uint64_t start= 0;
        volatile uint64_t end = 0;

        while (1) {
#pragma HLS PIPELINE II=1
            diff_time_pkg_t start_pkg;
            diff_time_pkg_t end_pkg;
            if (state == 0u)
            {
                if (tag_start.read_nb(start_pkg)){
                    start = start_pkg.data.ap_member(diff_time_t, cycle);
                    state = 1u;
                }
            }
            else
            {
                if (tag_end.read_nb(end_pkg))
                {
                    end = end_pkg.data.ap_member(diff_time_t, cycle);
                    state = 0u;
                    diff_time_pkg_t diff_pkg;
                    diff_pkg.data.ap_member(diff_time_t, cycle) = end - start;
                    diff_pkg.data.ap_member(diff_time_t, id) =   end_pkg.data.ap_member(diff_time_t, id);
                    time_out.write(diff_pkg);
                }
            }
        }
    }
}