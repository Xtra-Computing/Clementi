
#include "latency_common.h"


extern "C" {
    void cycle_record(ap_uint<64>  *out,
                      int size,
                      diff_time_stream_t &time_in)
    {
#pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem0
#pragma HLS INTERFACE s_axilite port = size
#pragma HLS INTERFACE s_axilite port = return
        uint32_t burstbuffer = 0;

#define MAX_RECORDS_BITS (12)
#define MAX_RECORDS (1<< MAX_RECORDS_BITS)

        ap_uint<64>  res[MAX_RECORDS];
#pragma HLS BIND_STORAGE variable=res type=ram_t2p
#pragma HLS DEPENDENCE variable=res intra WAR false

main: for (int i = 0; i < size; i++) {
#pragma HLS PIPELINE II=1
            ap_uint<32>  res_addr = i;
            diff_time_pkg_t time_pkg = time_in.read();
            res[res_addr.range(MAX_RECORDS_BITS - 1, 0)] = time_pkg.data.ap_member(diff_time_t, cycle);
        }
write_back: for (int i = 0; i <  MAX_RECORDS; i ++){
#pragma HLS PIPELINE II=1
            out[i] = res[i];
        }

    }
}