#include <hls_stream.h>
#include "../header/fpga_global_mem.h"
#include "../graph_fpga.h"

extern "C" {
    void  writeProp(
        uint16                  *vertexPropMem,
        hls::stream<burst_dest> &vertexUpdate,
        unsigned int            packet_num
    )
    {
#pragma HLS INTERFACE m_axi port=vertexPropMem offset=slave bundle=gmem0 \
                        max_write_burst_length=32 num_write_outstanding=32

#pragma HLS INTERFACE s_axilite port=vertexPropMem  bundle=control
#pragma HLS INTERFACE s_axilite port=packet_num      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        int address;
        int offset = 0;
        int process_packet_num = 0;
        burst_dest packet_in;

        enum STATE {firstFrame = 0, remainFrame = 1};
        STATE currentState = firstFrame;

        do {
            switch (currentState) {
                case firstFrame: {
                    vertexUpdate.read(packet_in); // first frame
                    address =  ((packet_in.data) >> 32) & 0xFFFFFFFF; // last 64 -32 bits 
                    offset = 0;
                    currentState = remainFrame;
                    break;
                }
                case remainFrame: {
                    vertexUpdate.read(packet_in); // next 21 frames;
                    vertexPropMem[address + offset] = packet_in.data; // write to memory;
                    offset ++;

                    if ((packet_in.last) == 1) {
                        // last frame 
                        currentState = firstFrame;
                        process_packet_num ++;
                    }
                    break;
                }
            }
        } while (process_packet_num < packet_num);

        return;
    }
}


