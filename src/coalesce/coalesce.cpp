#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "gp_type.h"
#include "stream_operation.h"

#ifndef LOG_CACHEUPDATEBURST
#error "LOG_CACHEUPDATEBURST not defined! Please define it in VPP_FLAGS"
#endif

#define LOG2_SIZE_BY_INT        (4) 




void src_to_address(  source_vertex_inner_stream_t &in,
                      single_source_vertex_frp_inner_stream_t (&frp_out)[16] )
{
#pragma HLS interface ap_ctrl_none port=return

src_to_address: while (1) {
        source_vertex_item_t src_item = in.read();
        for (int i = 0; i < 16; i++)
        {
#pragma HLS UNROLL
            single_source_vertex_frp_item_t single_item;
            single_item.ap_frp_data(single_source_vertex_t) =
                (src_item.ap_member_array(source_vertex_t, data, i)) >> (LOG_CACHEUPDATEBURST + LOG2_SIZE_BY_INT);
            single_item.ap_frp_last(single_source_vertex_t) = 1;
            frp_out[i].write(single_item );
        }
    }
}

void single_to_cmd(  single_source_vertex_frp_inner_stream_t &in,
                     cache_command_inner_stream_t  &out)
{
#pragma HLS interface ap_ctrl_none port=return

single_to_cmd: while (1) {
        single_source_vertex_frp_item_t src_frp = in.read();
        single_source_vertex_item_t src_item = src_frp.ap_frp_data(single_source_vertex_t);
        cache_command_item_t cmd;
        cmd.ap_member(cache_command_t, idx) = (src_item.ap_member(single_source_vertex_t, data));
        cmd.ap_member(cache_command_t, size) = 1;
        out.write(cmd);
    }
}

enum  COAL_STATE {ST_SRC1 = 0, ST_SRC2};

template<const int NC, int L>
void merge_same_item( single_source_vertex_frp_inner_stream_t &in1,
                      single_source_vertex_frp_inner_stream_t &in2,
                      single_source_vertex_frp_inner_stream_t &out )
{
#pragma HLS interface ap_ctrl_none port=return


    volatile uint32_t src1_data;
    volatile uint32_t src2_data;

    volatile uint8_t src1_valid = 0;
    volatile uint8_t src2_valid = 0;

    volatile uint8_t src1_last;

    enum  COAL_STATE {ST_SRC1 = 0, ST_SRC2};

    volatile uint8_t curr_state = ST_SRC1;



filter: while (1) {

        if (src1_valid == 0) {
            single_source_vertex_frp_item_t src1;
            if (in1.read_nb(src1))
            {
                src1_data = src1.ap_frp_data(single_source_vertex_t);
                src1_last = src1.ap_frp_last(single_source_vertex_t);
                src1_valid = 1;
            }
        }
        if (src2_valid == 0) {
            single_source_vertex_frp_item_t src2;
            if (in2.read_nb(src2))
            {
                src2_data = src2.ap_frp_data(single_source_vertex_t);
                src2_valid = 1;
            }
        }
        switch (curr_state)
        {
        case ST_SRC1:
        {
            if (src1_valid == 1 && src2_valid == 1)
            {
                single_source_vertex_frp_item_t frp_out;
                frp_out.ap_frp_data(single_source_vertex_t) = src1_data;
                if (src1_last == 0) {
                    frp_out.ap_frp_last(single_source_vertex_t) = 0;
                    curr_state = ST_SRC1;
                }
                else
                {
                    if (src1_data == src2_data) {
                        frp_out.ap_frp_last(single_source_vertex_t) = 1;
                        curr_state = ST_SRC1;
                        src2_valid = 0;
                    }
                    else {
                        frp_out.ap_frp_last(single_source_vertex_t) = 0;
                        curr_state = ST_SRC2;
                    }
                }
                src1_valid = 0;
                src1_last = 0;
                out.write(frp_out);

            }
            break;
        }
        case ST_SRC2:
        {
            if (src2_valid == 1)
            {
                single_source_vertex_frp_item_t frp_out;
                frp_out.ap_frp_data(single_source_vertex_t) = src2_data;
                frp_out.ap_frp_last(single_source_vertex_t) = 1;
                out.write(frp_out);
                curr_state = ST_SRC1;
                src2_valid = 0;
            }
            break;
        }
        }
    }
}


void filter_same_item( single_source_vertex_frp_inner_stream_t &in,
                       single_source_vertex_frp_inner_stream_t &out)
{
#pragma HLS interface ap_ctrl_none port=return

    enum  FLUSH_STATE {ST_INIT = 0, ST_RUNNING, ST_RESET};

    volatile uint32_t  last_item;
    uint8_t state = ST_INIT;


free_run: while (1)
    {
        switch (state)
        {
        case  ST_INIT:
        {
            single_source_vertex_frp_item_t  last;
            if (in.read_nb(last))
            {
                state = ST_RUNNING;
                last_item = last.ap_frp_data(single_source_vertex_t);
                out.write(last);
            }
            break;
        }
        case  ST_RUNNING:
        {
            single_source_vertex_frp_item_t  curr;
            if (in.read_nb(curr))
            {
                uint32_t curr_item = curr.ap_frp_data(single_source_vertex_t);
                if (curr_item != last_item)
                {
                    out.write(curr);
                    last_item = curr_item;
                }
            }
            break;
        }
        }
    }
}



extern "C" {
    void coalesce(      source_vertex_stream_t  &src_in,
                        cache_command_stream_t  &cmd_out )
    {
#pragma HLS interface ap_ctrl_none port=return

        static source_vertex_inner_stream_t src_innner;
#pragma HLS STREAM variable=src_innner  depth=2

        static single_source_vertex_frp_inner_stream_t single[16];
#pragma HLS STREAM variable=single  depth=16


        static single_source_vertex_frp_inner_stream_t inner_stream[15];
#pragma HLS STREAM variable=inner_stream  depth=16


        static single_source_vertex_frp_inner_stream_t out2;
#pragma HLS STREAM variable=out2  depth=2

        static cache_command_inner_stream_t cmd_inner;
#pragma HLS STREAM variable=cmd_inner  depth=2


#pragma HLS DATAFLOW
        axis_to_hls_stream(src_in, src_innner);
        src_to_address(src_innner, single);

        merge_same_item<0, 1>( single[0],  single[1],   inner_stream[0]);

        for (int i = 0; i < 14 ; i++)
        {
#pragma HLS UNROLL
            merge_same_item<1, 1>(        inner_stream[i],  single[i + 2],   inner_stream[i + 1]);
        }

        filter_same_item(inner_stream[14], out2);
        single_to_cmd(out2, cmd_inner);
        hls_to_axis_stream(cmd_inner, cmd_out);


    }
}
