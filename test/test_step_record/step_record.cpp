#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"

#define BIT_WIDTH   (512)

#define UNIT_SIZE   (128)
extern "C" {
    void step_record (  ap_uint<BIT_WIDTH>         *mem,
                        step_metadata_stream_t     &in,
                        step_metadata_stream_t     &out

                     ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_write_outstanding=64
        uint32_t addr = 0;
        ap_uint<1> end_flag = 0;
        while (1)
        {

            //for (int i = 0; i < WRITE_BURST_SIZE; i++)
            {
                res_data_item_t buffer_array[BIT_WIDTH / UNIT_SIZE];
#pragma HLS ARRAY_PARTITION variable=buffer_array dim=0

                for (int j = 0; j < BIT_WIDTH / UNIT_SIZE; j++)
                {
                    step_metadata_pkg_t pkg = in.read();

                    out.write(pkg);
                    res_data_item_t res_data;

                    res_data.ap_member(res_data_t, curr_vertex) = pkg.data.ap_member(step_metadata_t,curr_vertex);
                    res_data.ap_member(res_data_t, inner_state) = pkg.data.ap_member(step_metadata_t,inner_state);
                    res_data.ap_member(res_data_t, remain_step) = pkg.data.ap_member(step_metadata_t,assert);

                    buffer_array[j] = res_data;
                    if (pkg.last) {
                        end_flag = 1;
                        break;
                    }
                }
                ap_uint<BIT_WIDTH> out;
                for (int j = 0; j < BIT_WIDTH / UNIT_SIZE; j++)
                {
                    out.range(UNIT_SIZE * (j + 1) - 1, UNIT_SIZE * j) = buffer_array[j];
                }

                mem[addr] = out;
                addr ++;
                if (end_flag == 1)
                {
                    return;
                }

            }
        }
        //reduce_store<BIT_WIDTH>(size, mem, input);
    }
}
