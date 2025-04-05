// note that the globalTrans is the Aggregate sub-module in the paper. 
#include <hls_stream.h>
#include "../graph_fpga.h"
#include "../header/fpga_global_mem.h"

extern "C" {
    void  globalTrans (
        hls::stream<burst_dest> &vertexTrans,  // local vertex data, need to trans to other nodes
        hls::stream<burst_dest> &vertexUpdate, // local vertex data, need to update from other nodes
        hls::stream<burst_dest> &net_in,       // input data from network
        hls::stream<burst_dest> &net_out       // output data to next node
    ) {

#pragma HLS interface ap_ctrl_none port=return

        int current_node_id = -1; // default
        int write_read_num = 0;
        int FIFO_len = 32; // set packet threshold of (network write - read)
        burst_dest packet_in;
        burst_dest packet_wr; // for write kernel
        burst_dest trans_packet;

        enum STATE {firstFrame = 0, remainFrame_same = 1, remainFrame_other = 2, onlySend = 3};
        STATE currentState = firstFrame;

        while (1) {
            switch (currentState) {
                case firstFrame: {
                    // initial state;
                    if (!net_in.empty()) { // have data from network side;
                        net_in.read(packet_in); // read the first frame
                        int input_node_id = (packet_in.data) & 0xFFFFFFFF; // last 32 bits
                        packet_wr = packet_in;
                        vertexUpdate.write(packet_wr);

                        if (input_node_id == current_node_id) {
                            write_read_num --;
                            currentState = remainFrame_same; // no need forward
                        } else {
                            trans_packet.data = packet_in.data;
                            trans_packet.last = packet_in.last;
                            trans_packet.keep = -1;
                            trans_packet.dest = 1;
                            net_out.write(trans_packet); // need forward
                            currentState = remainFrame_other;
                        }

                    } else { // no data from network side
                        // write_read_num is used to aviod UDP data packet loss
                        if ((!vertexTrans.empty()) && (write_read_num < FIFO_len)) {
                            vertexTrans.read(trans_packet);
                            current_node_id = (trans_packet.data) & 0xFFFFFFFF; // last 32 bits
                            net_out.write(trans_packet);

                            write_read_num ++;
                            currentState = onlySend; // only send data, do not care net data
                        }
                    }

                    break;
                }
                case remainFrame_same: {
                    if (!net_in.empty()) { // only write
                        net_in.read(packet_in);
                        packet_wr = packet_in;
                        vertexUpdate.write(packet_wr);
                        if ((packet_in.last) == 1) {
                            currentState = firstFrame;
                        }
                    }
                    break;
                }
                case remainFrame_other: {
                    if (!net_in.empty()) { // write + forward
                        net_in.read(packet_in);
                        packet_wr = packet_in;
                        vertexUpdate.write(packet_wr);
                        trans_packet.data = packet_in.data;
                        trans_packet.last = packet_in.last;
                        trans_packet.keep = -1;
                        trans_packet.dest = 1;
                        net_out.write(trans_packet); // need forward
                        if ((packet_in.last) == 1) {
                            currentState = firstFrame;
                        }
                    }
                    break;
                }
                case onlySend: {
                    if (!vertexTrans.empty()) {
                        vertexTrans.read(trans_packet);
                        net_out.write(trans_packet);
                        if ((trans_packet.last) == 1) {
                            currentState = firstFrame;
                        }
                    }
                    break;
                }
            }
        }
        return;
    }
}