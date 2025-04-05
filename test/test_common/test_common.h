#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "stream_type.h"

template <int N>
void store_data (   int                                 size,
                    ap_uint<N>                          *mem,
                    hls::stream< ap_axiu< N, 0, 0, 0> > &input

                ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_write_outstanding=64

    for (int i = 0; i < size; i++) {
#pragma HLS PIPELINE II=1
        ap_axiu< N, 0, 0, 0>  pkg = input.read();

        mem[i] = pkg.data;
        if (pkg.last)
        {
            break;
        }
    }
}



template <int N>
void probe_data (   int                                 size,
                    ap_uint<N>                          *mem,
                    hls::stream< ap_axiu< N, 0, 0, 0> > &input,
                    hls::stream< ap_axiu< N, 0, 0, 0> > &output

                ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_write_outstanding=64

    for (int i = 0; i < size; i++) {
#pragma HLS PIPELINE II=1
        ap_axiu< N, 0, 0, 0>  pkg = input.read();
        output.write(pkg);

        mem[i] = pkg.data;
        if (pkg.last)
        {
            break;
        }
    }
}



template <int N>
void probe_data_with_label (    int                                 size,
                                ap_uint<N>                          *mem,
                                hls::stream< ap_axiu< N, 0, 0, 0> > &input,
                                hls::stream< ap_axiu< N, 0, 0, 0> > &output
                ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_write_outstanding=64
    ap_uint<32>  counter = 0;

    for (int i = 0; i < size; i++) {
#pragma HLS PIPELINE II=1
        ap_axiu< N, 0, 0, 0>  pkg = input.read();
        output.write(pkg);
        pkg.data.range(31,0) = counter;
        counter ++;

        mem[i] = pkg.data;
        if (pkg.last)
        {
            break;
        }
    }
}


template <int N>
void reduce_store (     int                                 size,
                        ap_uint<N>                          *mem,
                        hls::stream< ap_axiu< N, 0, 0, 0> > &input
                  ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_write_outstanding=64
    ap_uint<N> counter = 0;
    while (1) {
#pragma HLS PIPELINE II=1
        ap_axiu< N, 0, 0, 0>  pkg = input.read();
        counter += pkg.data;
        if (pkg.last)
        {
            break;
        }
    }
    mem[0] =  counter;
}

#define BURST_BUFFER_SIZE (1)
const int max_burst_length = BURST_BUFFER_SIZE;


template <typename stream_t, typename item_t, typename pkg_t>
void test_mem_loader(   int         size,
                        item_t      *mem,
                        stream_t    &output
                    ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_read_outstanding=2 max_read_burst_length=max_burst_length

    item_t buffer[BURST_BUFFER_SIZE];

    for (int i = 0; i < size; i += BURST_BUFFER_SIZE) {

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            buffer[j] = mem[i + j];
        }

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            pkg_t pkg;
            pkg.data = buffer[j];
            pkg.last = ((i + j) == (size - 1));
            output.write(pkg);
        }
    }
}



template <typename stream_t, typename item_t, typename pkg_t>
void test_mem_loader_end_ctrl(  int         size,
                                item_t      *mem,
                                stream_t    &output,
                                ctrl_stream_t  & ctrl
                    ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_read_outstanding=2 max_read_burst_length=max_burst_length

    item_t buffer[BURST_BUFFER_SIZE];

    for (int i = 0; i < size; i += BURST_BUFFER_SIZE) {

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            buffer[j] = mem[i + j];
        }

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            pkg_t pkg;
            pkg.data = buffer[j];
            pkg.last = ((i + j) == (size - 1));
            output.write(pkg);
        }
    }
    ctrl_pkg_t ctrl_pkg;
    ctrl_pkg.data = CTRL_END_OF_STREAM;
    ctrl_pkg.last = 1;
    ctrl.write(ctrl_pkg);
}



template <typename stream_t, typename item_t, typename pkg_t>
void test_mem_loader_end_token(  int         size,
                                item_t      *mem,
                                stream_t    &output
                    ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_read_outstanding=2 max_read_burst_length=max_burst_length

    item_t buffer[BURST_BUFFER_SIZE];

    for (int i = 0; i < size; i += BURST_BUFFER_SIZE) {

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            buffer[j] = mem[i + j];
        }

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            pkg_t pkg;
            pkg.data = buffer[j];
            pkg.last = 0;
            output.write(pkg);
        }
    }
    pkg_t pkg;
    item_t temp_data = 0;
    pkg.data =  temp_data - 1;
    pkg.last = 1;
    output.write(pkg);
}






template <typename stream_t, typename item_t, typename pkg_t>
void dummy_mem_loader(   int         size,
                        item_t      *mem,
                        stream_t    &output
                    ) {
#pragma HLS INTERFACE m_axi port=mem offset=slave bundle=gmem num_read_outstanding=2 max_read_burst_length=max_burst_length

    item_t buffer[BURST_BUFFER_SIZE];

    for (int i = 0; i < size; i += BURST_BUFFER_SIZE) {

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            buffer[j] = j;
        }

        for (int j = 0; j < BURST_BUFFER_SIZE; j++)
        {
            pkg_t pkg;
            pkg.data = buffer[j];
            pkg.last = ((i + j) == (size - 1));
            output.write(pkg);
        }
    }
    mem[0] = size;
}




template <int N>
void axis_padding (  hls::stream< ap_axiu< N, 0, 0, 0> > &input,
                hls::stream< ap_axiu< N, 0, 0, 0> > &output
                ) {
#pragma HLS interface ap_ctrl_none port=return
    while(true){
#pragma HLS PIPELINE II=1
        ap_axiu< N, 0, 0, 0>  pkg = input.read();
        output.write(pkg);
    }
}
