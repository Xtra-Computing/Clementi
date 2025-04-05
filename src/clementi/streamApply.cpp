#include <hls_stream.h>
#include "../graph_fpga.h"
#include "../header/fpga_global_mem.h"
#include "../header/fpga_apply.h"
#include "../header/fpga_application.h"

extern "C" {
    void  streamApply(
        uint16                  *vertexProp,
        hls::stream<burst_dest>  &input_from_merge, // input -- stream data after merge
        hls::stream<burst_dest>  &output_to_forward, // output -- stream data after apply, Forward in.
#if HAVE_APPLY_OUTDEG
        uint16                  *outDegree,
#endif
        int                     *outReg,
        unsigned int            addrOffset,
        unsigned int            vertexNum,
        unsigned int            argReg
    )
    {

#pragma HLS INTERFACE m_axi port=outReg offset=slave bundle=gmem0 \
                        max_read_burst_length=64 num_read_outstanding=32 \
                        max_write_burst_length=64 num_write_outstanding=32
#pragma HLS INTERFACE s_axilite port=outReg bundle=control


#pragma HLS INTERFACE m_axi port=vertexProp offset=slave bundle=gmem0 \
                        max_read_burst_length=64 num_read_outstanding=32 \
                        max_write_burst_length=64 num_write_outstanding=32
#pragma HLS INTERFACE s_axilite port=vertexProp bundle=control

#if HAVE_APPLY_OUTDEG

#pragma HLS INTERFACE m_axi port=outDegree offset=slave bundle=gmem1 \
                        max_read_burst_length=64 num_read_outstanding=32 \
                        max_write_burst_length=64 num_write_outstanding=32
#pragma HLS INTERFACE s_axilite port=outDegree bundle=control

        static hls::stream<burst_raw>      outDegreeStream;
#pragma HLS stream variable=outDegreeStream depth=256
        burstReadLiteRaw(addrOffset, vertexNum, outDegree, outDegreeStream);

#endif

#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=argReg         bundle=control
#pragma HLS INTERFACE s_axilite port=addrOffset     bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

#pragma HLS DATAFLOW

        static hls::stream<burst_raw>      vertexPropStream;
#pragma HLS stream variable=vertexPropStream depth=128

        int loopNum = (vertexNum >> 4);

        burstReadLiteRaw(addrOffset, vertexNum, vertexProp, vertexPropStream);

        applyFunction(
            loopNum,
#if HAVE_APPLY_OUTDEG
            outDegreeStream,
#endif
            vertexPropStream,
            input_from_merge,
            argReg,
            output_to_forward,
            outReg
        );

    }

}