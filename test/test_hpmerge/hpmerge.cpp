#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"

extern "C" {
    void hpmerge (adjacent_stream_t              &in1,
                            adjacent_stream_t              &in2,
                            adjacent_stream_t              &out) {
#pragma HLS interface ap_ctrl_none port=return


        adjacent_pkg_t in1_pkg;
        volatile uint8_t in1_flag = 0;

        adjacent_pkg_t in2_pkg;
        volatile uint8_t in2_flag = 0;


merge_main: while (1)
        {

            if ((in1_flag == 0) && (in1.read_nb(in1_pkg))) {
                in1_flag = 1;
            }

            if ((in2_flag == 0) && (in2.read_nb(in2_pkg))) {
                in2_flag = 1;
            }

            if ((in1_flag == 1) && (in1_flag == 1))
            {
                adjacent_pkg_t out_pkg;
                out_pkg.data = in1_pkg.data + in2_pkg.data;
                out.write(out_pkg);

                in1_flag = 0;
                in2_flag = 0;
            }
        }
    }
}

