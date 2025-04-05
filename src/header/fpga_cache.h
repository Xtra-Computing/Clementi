#ifndef __FPGA_CACHE_H__
#define __FPGA_CACHE_H__

#include "gp_type.h"
#include "../stream_operation.h"

#include "../graph_fpga.h"




void srcPropertyProcess( hls::stream<burst_token>           &dest_vertex_stream,
                         source_vertex_stream_t             &src_prop_stream,
                         hls::stream<edge_tuples_t>         &edge_tuple_stream
                       )
{
    burst_token     last_dest;
    read_from_stream(dest_vertex_stream , last_dest);
src_prop:while (true)
    {
#pragma HLS PIPELINE
        burst_token     dest_vertices;
        read_from_stream(dest_vertex_stream , dest_vertices);
        source_vertex_pkg_t prop = src_prop_stream.read();


        for (int j = 0; j < 2; j++)
        {

            edge_tuples_t out_tuple;
            for (int i =0; i < 8; i++)
            {
#pragma HLS UNROLL
                out_tuple.data[i].x =  last_dest.data.range( (( i ) << INT_WIDTH_SHIFT) + (j * 256) + 31,
                                                        (( i ) << INT_WIDTH_SHIFT) + (j * 256) +  0);
                out_tuple.data[i].y =  prop.data.ap_member_array(source_vertex_t, data, (i + (j * 8)));
            }
            if (dest_vertices.flag == FLAG_SET)
                out_tuple.flag = FLAG_SET;
            else
                out_tuple.flag = FLAG_RESET;
            edge_tuple_stream.write(out_tuple);
        }
        last_dest = dest_vertices;

        if(dest_vertices.flag == FLAG_SET)
        {
            break;
        }
    }
    clear_stream(src_prop_stream);
}


#endif /* __FPGA_CACHE_H__ */

