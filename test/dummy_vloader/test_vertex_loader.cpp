#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"

typedef ap_uint<512>      uint32x16_t;
typedef ap_uint<32>       uint32x1_t;

unsigned int minRand(unsigned int seed, int load) {
#pragma HLS inline off
    static ap_uint<32> lfsr;

    if (load == 1) lfsr = seed;
    bool b_32 = lfsr.get_bit(32 - 32);
    bool b_22 = lfsr.get_bit(32 - 22);
    bool b_2 = lfsr.get_bit(32 - 2);
    bool b_1 = lfsr.get_bit(32 - 1);
    bool new_bit = b_32 ^ b_22 ^ b_2 ^ b_1;
    lfsr = lfsr >> 1;
    lfsr.set_bit(31, new_bit);

    return lfsr.to_uint();
}


void rng_output(int size, hls::stream<uint32x1_t> &rng)
{
    minRand(16807, 1);
    rng_loop:for (int i = 0; i < size; i++)
    {
#pragma HLS PIPELINE II=1
        uint32x1_t rng_data = minRand(31, 0);
        rng.write(rng_data);
    }
}

void dummy_vd( int size, hls::stream<uint32x1_t> &rng, vertex_descriptor_stream_t &out, int degree)
{
    int const_value =  degree;
        for (int i = 0; i < size; i++) {
#pragma HLS PIPELINE II=1
            vertex_descriptor_pkg_t v_pkg;
            uint32x1_t rn_size = rng.read();

            uint32_t temp_size = rn_size & 0x7ff;
#ifdef SWITCH_TEST
            if (degree == 0)
            {
                v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048 + i;
                v_pkg.data.ap_member(vertex_descriptor_t,size) = (temp_size > i) ? (temp_size - i) : temp_size;
            }
            else if (degree == 3)
            {
                v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048;
                v_pkg.data.ap_member(vertex_descriptor_t,size) = (i & 0x1)? (16):1;
            }
            else if (degree == 4)
            {
                v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048;
                v_pkg.data.ap_member(vertex_descriptor_t,size) = (i & 0x1)? (16):32;
            }
            else if (degree == 5)
            {
                v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048;
                v_pkg.data.ap_member(vertex_descriptor_t,size) = (i & 0x1)? (16):32 * 16;
            }
            else if (degree == 6)
            {
                v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048;
                v_pkg.data.ap_member(vertex_descriptor_t,size) = (i & 0x1)? (32):32 * 16;
            }
            else
            {
                v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048;
                v_pkg.data.ap_member(vertex_descriptor_t,size) = const_value;
            }
#else
            v_pkg.data.ap_member(vertex_descriptor_t,start) = i * 2048;
            v_pkg.data.ap_member(vertex_descriptor_t,size)  = const_value;
#endif

            v_pkg.last = (i == (size - 1));
            out.write(v_pkg);
        }

}

extern "C" {
    void test_vertex_loader(int                         size,
                            vertex_descriptor_item_t    *vertex,
                            int                         degree,
                            vertex_descriptor_stream_t  &output

                      ) {
#pragma HLS INTERFACE m_axi port=vertex offset=slave bundle=gmem num_read_outstanding=64
#pragma HLS INTERFACE s_axilite port = size
#pragma HLS INTERFACE s_axilite port = degree
#pragma HLS INTERFACE s_axilite port = return

#pragma HLS dataflow
        hls::stream<uint32x1_t> rng;
        rng_output(size, rng);
        dummy_vd(size, rng, output, degree);

    }
}
