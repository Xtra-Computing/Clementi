#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>


#include "gp_type.h"
#include "stream_operation.h"

#include "graph_fpga.h"

#ifndef LOG_CACHEUPDATEBURST
#error "LOG_CACHEUPDATEBURST not defined! Please define it in VPP_FLAGS"
#endif


#define CACHE_UPDATE_BURST      (1<<LOG_CACHEUPDATEBURST)
#define LOG2_SIZE_BY_INT        (4)

#define CA_WIDTH                (32)
//(32 + 1 - LOG_CACHEUPDATEBURST - LOG2_SIZE_BY_INT)


typedef  struct
{
    uint_raw idx;
    uint_raw size;
    ap_uint<1> flag;
} cache_command;


#include "gp_type.h"
#include "stream_operation.h"




void command_convert(
    cache_command_stream_t          &cmd_in,
    //ctrl_stream_t                   &ctrl,
    hls::stream<cache_command>      &cmd_out)
{
    volatile uint32_t last_idx  = 0;
command_convert:    while (true)
    {
        cache_command_pkg_t pkg;
        if (cmd_in.read_nb(pkg))
        {
            {
                cache_command cmd;
                if (pkg.data.ap_member(cache_command_t, idx)
                        == (0xFFFFFFFF >> (LOG_CACHEUPDATEBURST + LOG2_SIZE_BY_INT)))
                {
                    cmd.idx = last_idx;
                    cmd.flag = FLAG_SET;
                    cmd.size = pkg.data.ap_member(cache_command_t, size);
                    write_to_stream(cmd_out, cmd);
                    return;

                }
                else
                {
                    cmd.idx = pkg.data.ap_member(cache_command_t, idx);
                    cmd.flag = FLAG_RESET;
                    last_idx = pkg.data.ap_member(cache_command_t, idx);
                    cmd.size = pkg.data.ap_member(cache_command_t, size);
                    write_to_stream(cmd_out, cmd);
                }

            }
        }
    }
}


void  read_with_request (   ap_uint<512>                    *input,
                            hls::stream<cache_command>      &cmd_in,
                            cache_line_stream_t             &data_out)
{

    volatile uint32_t last_addr = 0;
    burst_raw read_buffer[CACHE_UPDATE_BURST];
read_with_request:    while (true)
    {
        cache_command cmd;
        if (cmd_in.read_nb(cmd))
        {
            if (cmd.flag == FLAG_SET)
            {
                break;
            }
            //for (uint_raw i = 0; i < cmd.size ; i ++)
            int i = 0;
            {
                uint_raw outer_idx = (i + cmd.idx) & (((2 * 1024 * 1024 * 1024) >> LOG_CACHEUPDATEBURST) - 1);
                uint_raw address = ((uint_raw)(outer_idx) << (LOG_CACHEUPDATEBURST + LOG2_SIZE_BY_INT));
                for (int inner_idx = 0 ; inner_idx < CACHE_UPDATE_BURST; inner_idx ++) {
#pragma HLS PIPELINE II=1
                    read_buffer[inner_idx] = input[((uint_raw)(outer_idx) << LOG_CACHEUPDATEBURST) + inner_idx];
                    cache_line_pkg_t  cache_data;
                    cache_data.data.ap_member(cache_line_t, addr) = address + (inner_idx << 4);
                    cache_data.data.ap_member(cache_line_t, data)  = read_buffer[inner_idx];
                    cache_data.last = 0;
                    data_out.write(cache_data);
                    if (CACHE_UPDATE_BURST - 1 == inner_idx)
                    {
                        last_addr = address + (inner_idx << 4);
                    }
                }
            }
        }
    }

    {
        cache_line_pkg_t  cache_data;
        cache_data.data.ap_member(cache_line_t, addr) = (last_addr + (1 << 4));
        cache_data.data.ap_member(cache_line_t, data)  = 0;
        cache_data.last = 0;
        data_out.write(cache_data);
    }

    {
        cache_line_pkg_t  cache_data;
        cache_data.data.ap_member(cache_line_t, addr) = (last_addr + (2 << 4));
        cache_data.data.ap_member(cache_line_t, data)  = 0;
        cache_data.last = 1;
        data_out.write(cache_data);
    }
}





extern "C" {
    void src_cache_read_engine (
        ap_uint<512>                     *input,
        cache_command_stream_t           &cmd_in,
        cache_line_stream_t              &out
    )
    {
#pragma HLS INTERFACE m_axi port = input offset = slave bundle = gmem num_read_outstanding=32 max_read_burst_length=32

        hls::stream<cache_command>    cmd_stream;
#pragma HLS stream variable=cmd_stream  depth=16

#pragma HLS DATAFLOW

        command_convert(cmd_in, cmd_stream);
        read_with_request(input, cmd_stream, out);

    }
}

#if 0

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