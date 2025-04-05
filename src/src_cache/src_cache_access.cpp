#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>


#include "gp_type.h"
#include "stream_operation.h"

#include "graph_fpga.h"

#define URAM_DEPTH              (4096)

#define NUM_OF_URAM             (8)
#define CACHE_SIZE              (URAM_DEPTH * NUM_OF_URAM * 2)

#define CACHE_ADDRESS_MASK      ((CACHE_SIZE) - 1)

#define CA_WIDTH                (32)

#define NUM_OF_PE               (8)
//(32 + 1 - LOG_CACHEUPDATEBURST - LOG2_SIZE_BY_INT)


typedef  struct
{
    uint_raw idx;
    uint_raw size;
    ap_uint<1> flag;
} cache_command;


#include "gp_type.h"
#include "stream_operation.h"



#define CACHE_ACCESS_FRP(_src,_last)     {                                          \
        uint32_t address = (_src & CACHE_ADDRESS_MASK) >> 4;                        \
        uint32_t bit =  ((_src & CACHE_ADDRESS_MASK) >> 1) & (NUM_OF_URAM - 1);     \
        uint_uram tmp;                                                              \
        tmp = prop[bit][address];                                                   \
        if (_src & 0x01)                                                            \
            prop_value = tmp.range(63, 32);                                         \
        else                                                                        \
            prop_value = tmp.range(31,  0);                                         \
}

#define CACHE_SEND_OUT(_src, _last) {                                               \
        single_source_vertex_frp_item_t  out_pkg;                                   \
        out_pkg.ap_frp_data(single_source_vertex_t) = prop_value;                   \
        out_pkg.ap_frp_last(single_source_vertex_t) = _last;                        \
        out.write(out_pkg);                                                         \
        if (frp_flag == FLAG_SET) {end_of_sv = FLAG_SET;}                           \
        sv_ready = FLAG_RESET;                                                      \
}

#define CACHE_UPDATE() {                                                            \
}



void src_cache_pe_frp(  single_source_vertex_frp_inner_stream_t &in,
                        single_source_vertex_frp_inner_stream_t &out,
                        cache_line_frp_inner_stream_t &cache_in,
                        cache_line_frp_inner_stream_t &cache_out
                     )
{
#pragma HLS interface ap_ctrl_none port=return

    uint_uram   prop[NUM_OF_URAM][URAM_DEPTH];
#pragma HLS ARRAY_PARTITION variable=prop dim=1 complete
#pragma HLS BIND_STORAGE variable=prop type=RAM_T2P impl=URAM

#pragma HLS dependence variable=prop inter false
#pragma HLS dependence variable=prop intra false


    uint_uram  temp_buffer[NUM_OF_URAM];
#pragma HLS ARRAY_PARTITION variable=temp_buffer dim=1 complete
    uint32_t  sv = 0;
    //volatile  ca_need_update = 0;

free_run: while (1)
    {
#pragma HLS PIPELINE off
        uint8_t  tb_valid = 0;
        uint32_t tb_index = 0;

        uint8_t   sv_ready = 0;


        uint32_t  min_vertex_index = 0;

        uint32_t  max_vertex_index_cp = 0;
        uint32_t  max_vertex_index_buffer_sv = 0;
        uint32_t  max_vertex_index_buffer_cu = 0;
        uint32_t  required_min_vertex_index = 0;

        uint8_t   frp_flag = 0;

        uint8_t   ca_updated = 0;


        uint8_t   end_of_sv = 0;
        uint8_t   end_of_ca = 0;

        uint32_t  prop_value;
        uint32_t  tb_index_buffer = 0;
        uint32_t  max_vertex_index = 0;



        ap_uint<1>     ca_need_update = 0;


src_cache_pe_frp: while (true)
        {
#pragma HLS PIPELINE II=1
            single_source_vertex_frp_item_t  src_frp;

            uint8_t   ready_to_send = 0;

            if (sv_ready == FLAG_RESET)
            {
                if (in.read_nb(src_frp))
                {
                    sv = src_frp.ap_frp_data(single_source_vertex_t);
                    frp_flag = src_frp.ap_frp_last(single_source_vertex_t);

                    required_min_vertex_index = ((src_frp.ap_frp_data(single_source_vertex_t)) & (~(15u)));
#if 1
                    single_source_vertex_item_t src = sv;
                    if ((src >= min_vertex_index)
                            && (src < max_vertex_index_buffer_sv))  // data in cache and at least two cycle after the tag data been written.
                    {
                        {
                            CACHE_ACCESS_FRP(src, frp_flag);
                        }
                        ready_to_send = FLAG_SET;
                    }
                    else
#endif
                    {
                        sv_ready = FLAG_SET;
                    }
                }
            }
            else
            {
                single_source_vertex_item_t src = sv;
                if ((src >= min_vertex_index)
                        && (src < max_vertex_index_buffer_sv))
                {
                    {
                        CACHE_ACCESS_FRP(src, frp_flag);
                    }
                    ready_to_send = FLAG_SET;

                }
            }

            if ((tb_valid == FLAG_RESET) || (end_of_sv == FLAG_SET))
            {
                cache_line_frp_item_t cl_pkg;
                if (cache_in.read_nb(cl_pkg))
                {
                    cache_out.write(cl_pkg);
                    cache_line_item_t cl = cl_pkg.ap_frp_data(cache_line_t);
                    tb_index = cl.ap_member(cache_line_t, addr);
                    if (cl_pkg.ap_frp_last(cache_line_t) == 1)
                    {
                        end_of_ca = FLAG_SET;
                    }
                    burst_raw buffer = cl.ap_member(cache_line_t, data);
#if 0
                    if (required_min_vertex_index + (CACHE_SIZE - 1) >= max_vertex_index_buffer_cu) //ready for writing into cache, immediately write out
                    {
                        uint32_t curr_tb_index = cl.ap_member(cache_line_t, addr);
                        uint32_t tag = (curr_tb_index & CACHE_ADDRESS_MASK) >> 4;
                        {
                            for (int k = 0; k < NUM_OF_URAM; k ++) {
#pragma HLS UNROLL
                                prop[k][tag] = buffer.range(63 +  (64 * k), 0 + (64 * k));
                            }
                        }
                        ca_updated = FLAG_SET;
                        tb_valid = FLAG_RESET;
                    }
                    else
#endif
                    {
                        tb_valid = FLAG_SET;
                    }
                    for (int k = 0; k < NUM_OF_URAM; k ++) {
#pragma HLS UNROLL
                        temp_buffer[k] = buffer.range(63 +  (64 * k), 0 + (64 * k));
                    }
                    // TODO: feature direct
                }
            }
            else
            {
                if (ca_need_update == FLAG_SET) //ready for writing into cache, wait for
                {
                    uint32_t curr_tb_index = tb_index;
                    uint32_t tag = (curr_tb_index & CACHE_ADDRESS_MASK) >> 4;
                    {
                        for (int k = 0; k < NUM_OF_URAM; k ++) {
#pragma HLS UNROLL
                            prop[k][tag] = temp_buffer[k];
                        }
                    }
                    ca_updated = FLAG_SET;
                    tb_valid = FLAG_RESET;
                    ca_need_update = FLAG_RESET;
                }
            }
            tb_index_buffer = tb_index;
            if(ready_to_send == FLAG_SET)
            {
                CACHE_SEND_OUT(src, frp_flag);
                ready_to_send = 0;
            }


            if (ca_updated == FLAG_SET)
            {
                uint32_t curr_tb_index = tb_index_buffer;
                if (max_vertex_index < (curr_tb_index & (~0xf)) + 16)
                {
                    max_vertex_index = (curr_tb_index & (~0xf)) + 16;
                    max_vertex_index_cp = (curr_tb_index & (~0xf)) + 16;
                }

                if (curr_tb_index == required_min_vertex_index)
                {
                    min_vertex_index = tb_index_buffer;
                }

                ca_updated = FLAG_RESET;
            }
            max_vertex_index_buffer_sv = max_vertex_index;
            max_vertex_index_buffer_cu = max_vertex_index_cp;
            if (required_min_vertex_index + (CACHE_SIZE) > max_vertex_index_buffer_cu)
            {
                ca_need_update = FLAG_SET;
            }



            if ((end_of_ca == FLAG_SET) && (end_of_sv == FLAG_SET))
            {
                break;
            }
        }
    }
    //clear_stream(cache_in);


}


void src_buffer(    source_vertex_stream_t  &in,
                    source_vertex_frp_inner_stream_t  &out)
{
#pragma HLS interface ap_ctrl_none port=return

free_run: while (true)
    {
#pragma HLS PIPELINE off
        source_vertex_pkg_t last_pkg;
        last_pkg = in.read();
buffer: while (true)
        {
            source_vertex_pkg_t  pkg;
            if (in.read_nb(pkg))
            {
                source_vertex_frp_item_t  frp_sv;
                source_vertex_item_t  sv_item;
                for (int i = 0; i < 16; i ++)
                {
#pragma HLS UNROLL
                    sv_item.ap_member_array(source_vertex_t, data, i)
                        = last_pkg.data.ap_member_array(source_vertex_t, data, (15 - i));
                }
                frp_sv.ap_frp_data(source_vertex_t) = sv_item;
                frp_sv.ap_frp_last(source_vertex_t) = pkg.last;
                out.write(frp_sv);

                if (pkg.last == 1)
                {
                    break;
                }
                last_pkg = pkg;
            }
        }
    }
    //clear_stream(in);
}

void src_dispatch(  source_vertex_frp_inner_stream_t &in,
                    single_source_vertex_frp_inner_stream_t  (&out)[NUM_OF_PE])
{
#pragma HLS interface ap_ctrl_none port=return
free_run: while (true)
    {
#pragma HLS PIPELINE off
dispatch: while (true)
        {
            source_vertex_frp_item_t  frp;
            //frp = in.read();

            if (in.read_nb(frp))
            {
                source_vertex_item_t  sv_item = frp.ap_frp_data(source_vertex_t);
                for (int i = 0; i < NUM_OF_PE; i ++)
                {
#pragma HLS UNROLL
                    single_source_vertex_frp_item_t  frp_sv;
                    frp_sv.ap_frp_data(single_source_vertex_t) = sv_item.ap_member_array(source_vertex_t, data,  2 * i + 1);
                    frp_sv.ap_frp_last(single_source_vertex_t) = 0;
                    out[i].write(frp_sv);
                }
                for (int i = 0; i < NUM_OF_PE; i ++)
                {
#pragma HLS UNROLL
                    single_source_vertex_frp_item_t  frp_sv;
                    frp_sv.ap_frp_data(single_source_vertex_t) = sv_item.ap_member_array(source_vertex_t, data,  2 * i );
                    frp_sv.ap_frp_last(single_source_vertex_t) = frp.ap_frp_last(source_vertex_t);
                    out[i].write(frp_sv);
                }
                if ( frp.ap_frp_last(source_vertex_t) == 1)
                {
                    break;
                }
            }
        }
    }
}



void src_merge( single_source_vertex_frp_inner_stream_t  (&in)[8],
                source_vertex_stream_t &out)
{
#pragma HLS interface ap_ctrl_none port=return
free_run: while (true)
    {
#pragma HLS PIPELINE off
        uint8_t end_flag = 0;
merge: while (true)
        {
            source_vertex_pkg_t  pkg;
            for (int i = 0; i < 8; i ++)
            {
#pragma HLS UNROLL
                single_source_vertex_frp_item_t  frp_sv;
                frp_sv  = in[i].read();
                pkg.data.ap_member_array(source_vertex_t, data, 15 - (2 * i + 1)) = frp_sv.ap_frp_data(single_source_vertex_t);
            }
            for (int i = 0; i < 8; i ++)
            {
#pragma HLS UNROLL
                single_source_vertex_frp_item_t  frp_sv;
                frp_sv  = in[i].read();
                pkg.data.ap_member_array(source_vertex_t, data, 15 - (2 * i)) = frp_sv.ap_frp_data(single_source_vertex_t);
                if (i == 7)
                {
                    pkg.last = frp_sv.ap_frp_last(single_source_vertex_t);
                    if (frp_sv.ap_frp_last(single_source_vertex_t) == 1)
                    {
                        end_flag = 1;
                    }
                }
            }
            out.write(pkg);
            if (end_flag == 1)
            {
                break;
            }
        }
    }
}



extern "C" {
    void src_cache_access (
        source_vertex_stream_t       &in,
        cache_line_stream_t          &cache_line_in,
        source_vertex_stream_t       &out

    )
    {
#pragma HLS interface ap_ctrl_none port=return

        source_vertex_frp_inner_stream_t     sv_frp;
#pragma HLS stream variable=sv_frp  depth=510

        single_source_vertex_frp_inner_stream_t     sv_stream[NUM_OF_PE];
#pragma HLS stream variable=sv_stream  depth=2

        single_source_vertex_frp_inner_stream_t     pad_sv_stream[NUM_OF_PE];
#pragma HLS stream variable=pad_sv_stream  depth=2

        cache_line_frp_inner_stream_t     cl_stream[NUM_OF_PE + 1];
#pragma HLS stream variable=cl_stream  depth=2


        single_source_vertex_frp_inner_stream_t     out_stream[NUM_OF_PE];
#pragma HLS stream variable=out_stream  depth=2

#pragma HLS DATAFLOW

        src_buffer(in, sv_frp);

        src_dispatch(sv_frp, sv_stream);

        for (int i = 0; i < NUM_OF_PE ; i++)
        {
#pragma HLS UNROLL
            //frp_pad_stream< FRP_TEMPLATE(single_source_vertex) >(sv_stream[i], pad_sv_stream[i]);
        }

        free_run_axis_to_frp_stream< FRP_TEMPLATE(cache_line) >(cache_line_in, cl_stream[0]);

        for (int i = 0; i < NUM_OF_PE ; i++)
        {
#pragma HLS UNROLL
            src_cache_pe_frp(sv_stream[i], out_stream[i], cl_stream[i], cl_stream[i + 1]);
        }

        free_run_frp_sink_stream< FRP_TEMPLATE(cache_line) > (cl_stream[NUM_OF_PE]);

        src_merge(out_stream, out);

    }
}

#if 0



extern "C" {
    void src_cache_access (
        single_source_vertex_stream_t    &in,
        cache_line_stream_t              &cache_line_in,
        single_source_vertex_stream_t    &out

    )
    {
#pragma HLS DATAFLOW
        src_cache_pe(in, out, cache_line_in);
    }
}


extern "C" {
    void src_cache_read_engine (
        ap_uint<512>                     *input,
        single_source_vertex_stream_t    &in,
        cache_command_stream_t           &cmd_in,
        single_source_vertex_stream_t    &out
    )
    {


        hls::stream<cache_command>    cmd_stream;
#pragma HLS stream variable=cmd_stream  depth=512

        hls::stream<cache_line>    cache_update_stream;
#pragma HLS stream variable=cache_update_stream  depth=512

        single_source_vertex_frp_inner_stream_t in_frp;
#pragma HLS stream variable=in_frp  depth=2

        single_source_vertex_frp_inner_stream_t out_frp;
#pragma HLS stream variable=out_frp  depth=2

#pragma HLS DATAFLOW

        command_convert(cmd_in, cmd_stream);
        read_with_request(input, cmd_stream, cache_update_stream);

        axis_to_frp_stream(in, in_frp);
        src_cache_pe(in_frp, out_frp, cache_update_stream);
        frp_to_axis_stream(out_frp, out);

    }
}

#endif