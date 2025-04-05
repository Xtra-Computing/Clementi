#include "latency_common.h"

extern "C" {
    void time_tag(time_tag_stream_t &tag,
                  diff_time_stream_t &time)
    {
#pragma HLS interface ap_ctrl_none port=return
        volatile uint64_t counter = 0;

        while (1) {
#pragma HLS PIPELINE II=1
            counter++;
            time_tag_pkg_t pkg;

            if (tag.read_nb(pkg))
            {
                diff_time_pkg_t time_pkg;
                time_pkg.data.ap_member(diff_time_t, id) = 0;
                time_pkg.data.ap_member(diff_time_t, cycle) = counter;
                time_pkg.data.ap_member_offset(diff_time_t, id, 0, 8) = pkg.data.ap_member(time_tag_t, id);
                time.write(time_pkg);
            }
        }
    }
}