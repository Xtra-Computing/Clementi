#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "stream_operation.h"



#define NUM_OF_WEIGHTS (16)




void partial_perfix_sum(weight_inner_stream_t       &w_in,
                        burst_sync_inner_stream_t   &bs,
                        weight_inner_stream_t       &w_out )
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp
    burst_sync_item_t   bs_data = bs.read();
    weight_item_t       w_data = w_in.read();
    uint32_t            pps_data[NUM_OF_WEIGHTS];
pps_init: for (uint32_t i = 0; i < NUM_OF_WEIGHTS ; i++)
    {
#pragma HLS UNROLL
        pps_data[i] = w_data.range(32 * (i + 1) - 1, 32 * i );
    }
    uint32_t            pps_data_out[NUM_OF_WEIGHTS];

#pragma HLS ARRAY_PARTITION variable=pps_data   dim=0
#pragma HLS ARRAY_PARTITION variable=pps_data_out  dim=0

    pps_data_out[0] = pps_data[0];
    for (int i = 1; i < NUM_OF_WEIGHTS; i++)
    {
#pragma HLS UNROLL
        pps_data_out[i] = pps_data_out[i - 1] + pps_data[i];
    }

    weight_item_t       w_data_out;

pps_done: for (uint32_t i = 0; i < NUM_OF_WEIGHTS ; i++)
    {
#pragma HLS UNROLL
        w_data_out.range(32 * (i + 1) - 1, 32 * i ) = pps_data_out[i];
    }
    w_out.write(w_data_out);


}

void dummy_update(  weight_inner_stream_t                   &w_in,
                    burst_sync_inner_stream_t               &bs,
                    next_vertex_ordered_inner_stream_t      &next_vertex)
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp
    weight_item_t      w_data = w_in.read();
    burst_sync_item_t  bs_data = bs.read();

    if (bs_data.ap_member(burst_sync_t, burst_id) & 0x8000)
    {
        next_vertex_ordered_item_t out_data;
        out_data.ap_member(next_vertex_ordered_t, vertex) = w_data.range(512 - 1, 512 - 32);
        out_data.ap_member(next_vertex_ordered_t, id)
            = bs_data.ap_member(burst_sync_t, id);
        next_vertex.write(out_data);
    }
}


template <typename T>
void stream_duplicator_frp(hls::stream<T> &in, hls::stream<T> &out1, hls::stream<T> &out2)
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp
    T data;
    read_from_stream(in, data);
    write_to_stream(out1, data);
    write_to_stream(out2, data);

}

template <int T>
void axis_to_hls_stream_frp_with_last( hls::stream< ap_axiu<T, 0, 0, 0> > &in, hls::stream< ap_uint<T + 1> > &out )
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp

    ap_axiu<T, 0, 0, 0> in_axis_data = in.read();
    ap_uint<T + 1> out_data;
    out_data.range(T - 1, 0) = in_axis_data.data;
    out_data.range(T,T) = in_axis_data.last;
    out.write(out_data);
}



template <int T>
void axis_to_hls_stream_frp( hls::stream< ap_axiu<T, 0, 0, 0> > &in, hls::stream< ap_uint<T> > &out )
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp

    ap_axiu<T, 0, 0, 0> in_axis_data = in.read();
    ap_uint<T> out_data;
    out_data = in_axis_data.data;
    out.write(out_data);
}


template <int T>
void hls_to_axis_stream_frp( hls::stream< ap_uint<T> > &in, hls::stream< ap_axiu<T, 0, 0, 0> > &out)
{
#pragma HLS interface ap_ctrl_none port=return
    ap_uint<T> data = in.read();
    ap_axiu<T, 0, 0, 0>  pkg;
    pkg.data = data;
    out.write(pkg);
}

void frp_pipeline_test( weight_frp_inner_stream_t                 &weight_input,
                        next_vertex_ordered_frp_inner_stream_t    &update)
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp
    weight_frp_item_t in_pkg = weight_input.read();
    next_vertex_ordered_frp_item_t out_pkg;
    out_pkg.ap_member(next_vertex_ordered_t, vertex) = in_pkg.range(31, 0);
    out_pkg.ap_member(next_vertex_ordered_t, id) = in_pkg.range(31 + 32, 0 + 32);
    out_pkg.ap_frp_last(next_vertex_ordered_t) = in_pkg.ap_frp_last(weight_t);

    update.write(out_pkg);
}



void dummy_mul_test( next_vertex_ordered_frp_inner_stream_t       &in,
                     next_vertex_ordered_stream_t    &out)
{
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS PIPELINE style=frp
    next_vertex_ordered_frp_item_t in_pkg = in.read();

    next_vertex_ordered_pkg_t out_pkg;
    out_pkg.data.ap_member(next_vertex_ordered_t, vertex)
        = in_pkg.ap_member(next_vertex_ordered_t, vertex) * in_pkg.ap_member(next_vertex_ordered_t, id);
    out_pkg.data.ap_member(next_vertex_ordered_t, id) = in_pkg.ap_member(next_vertex_ordered_t, id);
    out_pkg.last =in_pkg.ap_frp_last(next_vertex_ordered_t);
    out.write(out_pkg);
}


extern "C" {
    void weighted_sampler (
                            weight_stream_t                 &weight_input,
                            next_vertex_ordered_stream_t    &update
                          ) {
#pragma HLS interface ap_ctrl_none port=return


        weight_frp_inner_stream_t                   w_inner;
        next_vertex_ordered_frp_inner_stream_t      nv_inner;
        for (;;)
        {
#pragma HLS DATAFLOW
            axis_to_hls_stream_frp_with_last(weight_input, w_inner);
            frp_pipeline_test(w_inner, nv_inner);
            dummy_mul_test(nv_inner, update);
        }
    }
}

// R ~(0,1) < W/ perfix W
// R * perfix W <  W *( 2^32)

#if 0
extern "C" {
    void weighted_sampler ( burst_sync_stream_t             &burst_sync,
                            weight_stream_t                 &weight_input,
                            next_vertex_ordered_stream_t    &update
                          ) {
#pragma HLS interface ap_ctrl_none port=return

        for (;;)
        {
#pragma HLS DATAFLOW

            weight_inner_stream_t w;
#pragma HLS STREAM variable=w depth=3
            burst_sync_inner_stream_t  bs;
#pragma HLS STREAM variable=bs depth=2
            axis_to_hls_stream_frp(burst_sync, bs);
            axis_to_hls_stream_frp(weight_input, w);




            next_vertex_ordered_inner_stream_t nv;
#pragma HLS STREAM variable=nv depth=2

            burst_sync_inner_stream_t  bs_perfix, bx_sample;
#pragma HLS STREAM variable=bx_sample depth=16
#pragma HLS STREAM variable=bs_perfix depth=2

            weight_inner_stream_t w_out;
#pragma HLS STREAM variable=bs_perfix depth=2

            stream_duplicator_frp(bs, bs_perfix, bx_sample);
            partial_perfix_sum(w, bs_perfix, w_out);
            dummy_update(w_out, bx_sample, nv);

            hls_to_axis_stream_frp(nv, update);
        }
    }
}
#endif

#if 0

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

#endif
