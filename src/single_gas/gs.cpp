#include <hls_stream.h>
#include <string.h>

#include "../data_struct/gp_type.h"
#include "../stream_operation.h"
#include "../graph_fpga.h"

#include "../header/fpga_global_mem.h"
#include "../header/fpga_slice.h"
#include "../header/fpga_gather.h"
#include "../header/fpga_filter.h"
#include "../header/fpga_process_edge.h"
#include "../header/fpga_cache.h"
#include "../header/fpga_edge_prop.h"
#include "../header/fpga_application.h"

void src_stream_out(    hls::stream<burst_token>        &src_stream,
                        source_vertex_stream_t          &src_axis)
{
    while (true)
    {
        burst_token map;
        read_from_stream(src_stream, map);
        source_vertex_pkg_t pkg;
        pkg.data = map.data;
        pkg.last = 0;

        if (map.flag == FLAG_SET)
        {
            break;
        }
        src_axis.write(pkg);
    }
    source_vertex_pkg_t pkg;
    for (int i = 0; i < 16; i++)
    {
#pragma HLS UNROLL
        pkg.data.ap_member_array(source_vertex_t, data, i) = 0xFFFFFFFF;
    }
    pkg.last = 1;
    src_axis.write(pkg);
    return;
}


extern "C" {
    void  gs(
        uint16                          *edgesHeadArray,
        // uint16                       *tmpVertexProp,
        hls::stream<burst_dest>         &tmpVertexProp,
#if HAVE_EDGE_PROP == true
        uint16                          *edgeProp,
#endif
        int                             edge_end,
        int                             sink_offset,
        int                             sink_end,
        source_vertex_stream_t          &srcOut,
        source_vertex_stream_t          &propIn,
        hls::stream<int>                &perfStartStrm
    )
    {
        const int stream_depth_filter = QUEUE_SIZE_FILTER;

        const int stream_depth_memory = QUEUE_SIZE_MEMORY;
#pragma HLS DATAFLOW



#pragma HLS INTERFACE m_axi port=edgesHeadArray offset=slave bundle=gmem0 max_read_burst_length=64
#pragma HLS INTERFACE s_axilite port=edgesHeadArray bundle=control

// #pragma HLS INTERFACE m_axi port=tmpVertexProp offset=slave bundle=gmem0 max_write_burst_length=64 num_write_outstanding=4
// #pragma HLS INTERFACE s_axilite port=tmpVertexProp bundle=control


#if HAVE_EDGE_PROP

#pragma HLS INTERFACE m_axi port=edgeProp offset=slave bundle=gmem3 max_read_burst_length=32
#pragma HLS INTERFACE s_axilite port=edgeProp bundle=control

#endif
#pragma HLS INTERFACE s_axilite port=edge_end       bundle=control
#pragma HLS INTERFACE s_axilite port=sink_offset    bundle=control
#pragma HLS INTERFACE s_axilite port=sink_end       bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control



        hls::stream<int2_token>           buildArray[PE_NUM];
#pragma HLS stream variable=buildArray  depth=2

        hls::stream<int2_token>           buildArraySlice[PE_NUM];
#pragma HLS stream variable=buildArraySlice  depth=2

        hls::stream<edge_tuples_t>   edgeTuplesArray[PE_NUM];
#pragma HLS stream variable=edgeTuplesArray  depth=2

        hls::stream<filter_type>    toFilterArray[PE_NUM];
#pragma HLS stream variable=toFilterArray  depth=stream_depth_filter

        hls::stream<filter_type>    toFilterArraySlice[PE_NUM];
#pragma HLS stream variable=toFilterArraySlice  depth=2

        hls::stream<filter_type>    toFilterArraySlice2[PE_NUM];
#pragma HLS stream variable=toFilterArraySlice2  depth=2


        hls::stream<uint_uram>    writeArray[PE_NUM];
#pragma HLS stream variable=writeArray  depth=2

        hls::stream<uint_uram>    writeArrayLayer1[PE_NUM];
#pragma HLS stream variable=writeArrayLayer1  depth=2


        hls::stream<burst_token>      edgeBurstStream;
#pragma HLS stream variable=edgeBurstStream depth=512

        hls::stream<burst_token>      mapStream;
#pragma HLS stream variable=mapStream depth=2

#if HAVE_EDGE_PROP
        hls::stream<burst_token>      edgePropSliceStream;
#pragma HLS stream variable=edgePropSliceStream depth=2
        hls::stream<burst_token>      edgePropStream;
#pragma HLS stream variable=edgePropStream depth=2
        hls::stream<burst_token>      edgePropStreamTmp;
#pragma HLS stream variable=edgePropStreamTmp depth=stream_depth_memory

#endif

        hls::stream<edge_tuples_t>   edgeTuplesBuffer;
#pragma HLS stream variable=edgeTuplesBuffer depth=2


        edge_tuples_t tuples[PE_NUM];
#pragma HLS ARRAY_PARTITION variable=tuples dim=0 complete

        filter_type filter[PE_NUM];
#pragma HLS ARRAY_PARTITION variable=filter dim=0 complete

        uchar opcode[PE_NUM];
#pragma HLS ARRAY_PARTITION variable=opcode dim=0 complete

        shuffled_type shuff_ifo[PE_NUM];
#pragma HLS ARRAY_PARTITION variable=shuff_ifo dim=0 complete


        uint_uram tmpVPropBuffer[PE_NUM][(MAX_VERTICES_IN_ONE_PARTITION / 2) >> (LOG2_PE_NUM)];
#pragma HLS ARRAY_PARTITION variable=tmpVPropBuffer dim=1 complete
#pragma HLS RESOURCE variable=tmpVPropBuffer core=XPM_MEMORY uram



        filter_type                 filter_tmp[PE_NUM];
#pragma HLS ARRAY_PARTITION variable=filter_tmp dim=0 complete
        uint_raw                    filter_num[PE_NUM];
#pragma HLS ARRAY_PARTITION variable=filter_num dim=0 complete


        perfStartStrm.write(0); // Gs kernel start flag // end flag in steamWrite;
        //last token: flag = flag_set (1u), empty data
        burstRead(0, edge_end, edgesHeadArray, mapStream, edgeBurstStream);


        src_stream_out(mapStream, srcOut);

#if HAVE_EDGE_PROP
        burstReadProp(0, edge_end, edgeProp, edgePropStream);
        sliceStream(edgePropStream, edgePropStreamTmp);
        sliceStream(edgePropStreamTmp, edgePropSliceStream);

#endif

        srcPropertyProcess(edgeBurstStream, propIn, edgeTuplesBuffer);

#if HAVE_EDGE_PROP
        propProcess(edgePropSliceStream, edgeTuplesBuffer, edgeTuplesArray[0]);
#else
        propProcessSelf(edgeTuplesBuffer, edgeTuplesArray[0]);
#endif


        for (int i = 0; i < (PE_NUM - 1) ; i++)
        {
#pragma HLS UNROLL
            shuffleEntry (
                i,
                edgeTuplesArray[i],
                edgeTuplesArray[i + 1],
                toFilterArraySlice[i],
                filter[i],
                tuples[i],
                opcode[i],
                shuff_ifo[i]
            );
        }


        theLastShuffleEntry (
            (PE_NUM - 1),
            edgeTuplesArray[(PE_NUM - 1)],
            toFilterArraySlice[(PE_NUM - 1)],
            filter[(PE_NUM - 1)],
            tuples[(PE_NUM - 1)],
            opcode[(PE_NUM - 1)],
            shuff_ifo[(PE_NUM - 1)]
        );

        for (int i = 0; i < PE_NUM ; i++)
        {
#pragma HLS UNROLL
            filterSlice(toFilterArraySlice[i], toFilterArray[i]);
        }
        for (int i = 0; i < PE_NUM ; i++)
        {
#pragma HLS UNROLL
            filterSlice(toFilterArray[i], toFilterArraySlice2[i]);
        }

        for (int i = 0; i < PE_NUM ; i++)
        {
#pragma HLS UNROLL
            tupleFilter( filter_tmp[i], filter_num[i], toFilterArraySlice2[i], buildArray[i]);
        }


        for (int i = 0; i < PE_NUM ; i++)
        {
#pragma HLS UNROLL
            processEdgesBuildSlice(buildArray[i], buildArraySlice[i]);
        }

        for (int i = 0; i < PE_NUM ; i++)
        {
#pragma HLS UNROLL
            dstPropertyProcess(i, sink_offset, sink_end, tmpVPropBuffer[i], buildArraySlice[i], writeArrayLayer1[i]);
        }
        for (int i = 0; i < PE_NUM ; i++)
        {
#pragma HLS UNROLL
            processEdgesSlice(writeArrayLayer1[i], writeArray[i]);
        }

        processEdgeWrite(sink_offset, sink_end, writeArray, tmpVertexProp);
        
    }

} // extern C

