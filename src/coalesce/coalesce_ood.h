#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "gp_type.h"
#include "stream_operation.h"


#define LOG_CACHE_LINE_SIZE (8)
#define CACHE_LINE_SIZE (1<<LOG_CACHE_LINE_SIZE)
#define N_PE (16)




void src_to_address(  source_vertex_inner_stream_t &in,
                      single_source_vertex_inner_stream_t (&out)[N_PE] )
{
#pragma HLS interface ap_ctrl_none port=return

src_to_address: while (1) {
        source_vertex_item_t src_item = in.read();
        for (int i = 0; i < N_PE; i++)
        {
#pragma HLS UNROLL
            single_source_vertex_item_t single_item  = src_item.ap_member_array(source_vertex_t, data, i);
            out[i].write(single_item >> LOG_CACHE_LINE_SIZE);
        }
    }
}


template<const int NC, int L>
void filter_out_same( single_source_vertex_inner_stream_t &in1,
                      single_source_vertex_inner_stream_t &in2,
                      single_source_vertex_inner_stream_t &out )
{
#pragma HLS interface ap_ctrl_none port=return

filter: while (1) {
        source_vertex_item_t src1 = in1.read();
        source_vertex_item_t src2 = in2.read();
        if (src1 == src2) {
            out.write(src1);
        }
        else
        {
            out.write(src1);
            out.write(src2);
        }
    }
}




void single_to_cmd(  single_source_vertex_inner_stream_t &in,
                     cache_command_inner_stream_t  &out)
{
#pragma HLS interface ap_ctrl_none port=return

single_to_cmd: while (1) {
        single_source_vertex_item_t src_item = in.read();
        cache_command_item_t cmd;
        cmd.ap_member(cache_command_t, idx) = src_item.ap_member(single_source_vertex_t, data);
        cmd.ap_member(cache_command_t, size) = CACHE_LINE_SIZE;
        out.write(cmd);
    }
}

extern "C" {
    void coalesce(      source_vertex_stream_t  &src_in,
                        cache_command_stream_t  &cmd_out )
    {
#pragma HLS interface ap_ctrl_none port=return

        static source_vertex_inner_stream_t src_innner;
#pragma HLS STREAM variable=src_innner  depth=2

        static single_source_vertex_inner_stream_t single[N_PE];
#pragma HLS STREAM variable=single  depth=2

        static single_source_vertex_inner_stream_t level_1[8];
#pragma HLS STREAM variable=level_1  depth=4

        static single_source_vertex_inner_stream_t level_2[4];
#pragma HLS STREAM variable=level_2  depth=8

        static single_source_vertex_inner_stream_t level_3[2];
#pragma HLS STREAM variable=level_3  depth=16

        static single_source_vertex_inner_stream_t out1;
#pragma HLS STREAM variable=out1  depth=16


        static single_source_vertex_inner_stream_t out2;
#pragma HLS STREAM variable=out2  depth=16

        static cache_command_inner_stream_t cmd_inner;
#pragma HLS STREAM variable=cmd_inner  depth=2


#pragma HLS DATAFLOW
        axis_to_hls_stream(src_in, src_innner);
        src_to_address(src_innner, single);

        // level 1
        for (int i = 0 ; i < N_PE / 2 ; i++)
        {
#pragma HLS UNROLL
            filter_out_same<0, 1>(single[2 * i], single[2 * i + 1], level_1[i]);
        }

        for (int i = 0 ; i < 4 ; i++)
        {
#pragma HLS UNROLL
            filter_out_same<0, 2>(level_1[2 * i], level_1[2 * i + 1], level_2[i]);
        }

        for (int i = 0 ; i < 2 ; i++)
        {
#pragma HLS UNROLL
            filter_out_same<0, 3>(level_2[2 * i], level_2[2 * i + 1], level_3[i]);
        }

        filter_out_same<1,4>(level_3[0], level_3[1], out1);
        pad(out1,out2);

        single_to_cmd(out2, cmd_inner);
        hls_to_axis_stream(cmd_inner, cmd_out);


    }
}
