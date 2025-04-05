#ifndef __FPGA_GLOBAL_MEM_H__
#define __FPGA_GLOBAL_MEM_H__

#include "../graph_fpga.h"

#include "fpga_application.h"


#define WRITE_BACK_BURST_LENGTH     (64)
#define LOG_WRITE_BURST_LEN         (6)


template <typename T>
void burstReadProp(
    uint_raw                        edge_offset,
    uint_raw                        edge_end,
    uint16                          *input,
    hls::stream<T>                  &outputstream
)
{
#pragma HLS function_instantiate variable=input
#pragma HLS function_instantiate variable=outputstream
    unsigned int offset = 0;
    unsigned int end = ((edge_end / 2 - 1) >> (LOG2_SIZE_BY_INT + LOG_BURSTBUFFERSIZE)) + 1;


    burst_raw read_buffer[BURSTBUFFERSIZE];

    {

timeLineLoop : for (unsigned int i = (offset); i < (end); i ++)
        {
#if 0
            int chunk_size;
            if ((i + BURSTBUFFERSIZE * SIZE_BY_INT) > (end))
            {
                chunk_size = ((end) - i) >> LOG2_SIZE_BY_INT;
            }
            else
            {
                chunk_size = BURSTBUFFERSIZE;
            }
#else

#define  chunk_size  (BURSTBUFFERSIZE)

#endif

            for (int inner_idx = 0 ; inner_idx < chunk_size; inner_idx ++) {
#pragma HLS PIPELINE II=1
                read_buffer[inner_idx] = input[((uint_raw)(i) << LOG_BURSTBUFFERSIZE) + inner_idx];
            }

            for (int inner_idx = 0; inner_idx < chunk_size; inner_idx++)
            {
#pragma HLS PIPELINE II=1
                burst_token token;
                token.data = read_buffer[inner_idx];
                token.flag = FLAG_RESET;
                write_to_stream(outputstream, token);
            }
        }
    }

    {
        burst_token token;
        token.flag = FLAG_SET;
        write_to_stream(outputstream, token);
        return;
    }

    return;
}

void burstReadLiteRaw(
    uint_raw                        edge_offset,
    uint_raw                        edge_end,
    uint16                          *input,
    hls::stream<burst_raw>          &outstream
)
{
#pragma HLS function_instantiate variable=input
#pragma HLS function_instantiate variable=outstream

    unsigned int burst_read_size = 64;
    unsigned int offset = edge_offset  >> LOG2_SIZE_BY_INT;
    unsigned int end = ((edge_end - 1) >> (LOG2_SIZE_BY_INT + 6)) + 1; // burst size = 64;

    {

timeLineLoop : for (unsigned int i = 0; i < end; i ++)
        {
            for (unsigned int j = 0; j < burst_read_size; j++) {

#pragma HLS PIPELINE II=1
            burst_raw out_raw;
            out_raw = input[j + i * burst_read_size + offset];
            write_to_stream(outstream, out_raw);

            }
        }
    }
    return;
}

void burstReadLite(
    uint_raw                        edge_offset,
    uint_raw                        edge_end,
    uint16                          *input,
    hls::stream<burst_dest>          &outstream
)
{
#pragma HLS function_instantiate variable=input
#pragma HLS function_instantiate variable=outstream

    unsigned int offset = edge_offset  >> LOG2_SIZE_BY_INT;
    unsigned int end = ((edge_end - 1) >> (LOG2_SIZE_BY_INT)) + 1;

    unsigned int burst_read_size = 64;

    {

timeLineLoop : for (unsigned int i = (offset); i < (offset + (end / burst_read_size)); i ++)
        {
            for (unsigned int j = 0; j < burst_read_size; j++) {

#pragma HLS PIPELINE II=1
                burst_dest out_raw;
                out_raw.data = input[j + i * burst_read_size];
                write_to_stream(outstream, out_raw);

            }
        }
    }
    return;
}


template <typename T>
void dummyStream(
    uint_raw                        edge_end,
    hls::stream<T>                  &outputstream
)
{
#pragma HLS function_instantiate variable=outputstream
    unsigned int end = ((edge_end - 1) >> (LOG2_SIZE_BY_INT + LOG_BURSTBUFFERSIZE)) + 1;

    T zero = 0; // set 0 as dummy.

timeLineLoop : for (unsigned int i = 0; i < end; i ++)
        {

#define  chunk_size_lite  (BURSTBUFFERSIZE)

            for (int inner_idx = 0; inner_idx < chunk_size_lite; inner_idx++)
            {
#pragma HLS PIPELINE II=1
                write_to_stream(outputstream, zero);
            }
        }
    return;
}


template <typename T>
void burstRead(
    uint_raw                        edge_offset,
    uint_raw                        edge_end,
    uint16                          *input,
    hls::stream<T>                  &srcstream,
    hls::stream<T>                  &dststream
)
{
#pragma HLS function_instantiate variable=input
#pragma HLS function_instantiate variable=srcstream
#pragma HLS function_instantiate variable=dststream

    unsigned int offset = 0;
    unsigned int end = ((edge_end - 1) >> (LOG2_SIZE_BY_INT)) + 1;
    
    burst_token src_token;
    burst_token dst_token;

    {

timeLineLoop : for (unsigned int j = (offset); j < (end); j ++)
        {
#pragma HLS PIPELINE II=1
            burst_token token;
            token.flag = FLAG_RESET;
            token.data = input[j];

            for(int i = 0; i < 8; i ++)
            {
    #pragma HLS UNROLL
                src_token.data.range(256 * (j & 0x1) + 31 + (i << 5), 256 * (j & 0x1) + (i << 5)) = token.data.range(31 + (i << 6), (i << 6));
                dst_token.data.range(256 * (j & 0x1) + 31 + (i << 5), 256 * (j & 0x1) + (i << 5)) = token.data.range(63 + (i << 6), 32 + (i << 6));
            }
            
            if ((j & 0x1) == 1) {
                src_token.flag = FLAG_RESET;
                dst_token.flag = FLAG_RESET;
                write_to_stream(srcstream, src_token);
                write_to_stream(dststream, dst_token);
            }
        }
    }


    {   
        src_token.flag = FLAG_SET;
        dst_token.flag = FLAG_SET;
        write_to_stream(srcstream, src_token);
        write_to_stream(dststream, dst_token);
    }

    return;
}

template <typename T>
void burstReadPerf(
    uint_raw                        edge_offset,
    uint_raw                        edge_end,
    uint16                          *input,
    hls::stream<T>                  &srcstream,
    hls::stream<T>                  &dststream,
    hls::stream<int>                &startStrm
)
{
#pragma HLS function_instantiate variable=input
#pragma HLS function_instantiate variable=srcstream
#pragma HLS function_instantiate variable=dststream
#pragma HLS function_instantiate variable=startStrm

    startStrm.write(0);

    unsigned int offset = 0;
    unsigned int end = ((edge_end - 1) >> (LOG2_SIZE_BY_INT)) + 1;
    
    burst_token src_token;
    burst_token dst_token;

    {

timeLineLoop : for (unsigned int j = (offset); j < (end); j ++)
        {
#pragma HLS PIPELINE II=1
            burst_token token;
            token.flag = FLAG_RESET;
            token.data = input[j];

            for(int i = 0; i < 8; i ++)
            {
    #pragma HLS UNROLL
                src_token.data.range(256 * (j & 0x1) + 31 + (i << 5), 256 * (j & 0x1) + (i << 5)) = token.data.range(31 + (i << 6), (i << 6));
                dst_token.data.range(256 * (j & 0x1) + 31 + (i << 5), 256 * (j & 0x1) + (i << 5)) = token.data.range(63 + (i << 6), 32 + (i << 6));
            }
            
            if ((j & 0x1) == 1) {
                src_token.flag = FLAG_RESET;
                dst_token.flag = FLAG_RESET;
                write_to_stream(srcstream, src_token);
                write_to_stream(dststream, dst_token);
            }
        }
    }


    {   
        src_token.flag = FLAG_SET;
        dst_token.flag = FLAG_SET;
        write_to_stream(srcstream, src_token);
        write_to_stream(dststream, dst_token);
    }

    return;
}

void writeBack(int idx, uint16 *addr, hls::stream<burst_raw>  &input)
{
#pragma HLS function_instantiate variable=idx

    burst_raw ram[WRITE_BACK_BURST_LENGTH];

    unsigned int counter = 0;
    while (true)
    {

        for (int i = 0; i < WRITE_BACK_BURST_LENGTH; i++)
        {
#pragma HLS PIPELINE II=1
            burst_raw tmp;
            read_from_stream(input, tmp);
            if (tmp.range(BURST_ALL_BITS - 1, BURST_ALL_BITS - 32) == ENDFLAG)
            {
                return;
            }
            ram[i] = tmp;
        }
        for (int j = 0; j < WRITE_BACK_BURST_LENGTH; j++)
        {
#pragma HLS PIPELINE II=1
            addr[(counter << LOG_WRITE_BURST_LEN) + j] = ram[j];
        }
        counter ++;

    }
}

/*
template <typename T>
void burstReadLite(
    uint_raw                        edge_offset,
    uint_raw                        edge_end,
    uint16                          *input,
    hls::stream<T>                  &outputstream
)
{
#pragma HLS function_instantiate variable=input
#pragma HLS function_instantiate variable=outputstream
    unsigned int offset = edge_offset  >> (LOG2_SIZE_BY_INT + LOG_BURSTBUFFERSIZE);
    unsigned int end = ((edge_end - 1) >> (LOG2_SIZE_BY_INT + LOG_BURSTBUFFERSIZE)) + 1;


    T read_buffer[BURSTBUFFERSIZE];

    {

timeLineLoop : for (unsigned int i = (offset); i < (offset + end); i ++)
        {
#if 0
            int chunk_size;
            if ((i + BURSTBUFFERSIZE * SIZE_BY_INT) > (end))
            {
                chunk_size = ((end) - i) >> LOG2_SIZE_BY_INT;
            }
            else
            {
                chunk_size = BURSTBUFFERSIZE;
            }
#else

#define  chunk_size_lite  (BURSTBUFFERSIZE)

#endif

            for (int inner_idx = 0 ; inner_idx < chunk_size_lite; inner_idx ++) {
#pragma HLS PIPELINE II=1
                read_buffer[inner_idx] = input[((uint_raw)(i) << LOG_BURSTBUFFERSIZE) + inner_idx];
            }

            for (int inner_idx = 0; inner_idx < chunk_size_lite; inner_idx++)
            {
#pragma HLS PIPELINE II=1
                write_to_stream(outputstream, read_buffer[inner_idx]);
            }
        }
    }
    return;
}
*/


void writeBackLite(int totalSize, uint16 *addr, hls::stream<burst_raw>  &input)
{
#pragma HLS function_instantiate variable=addr

    unsigned int counter = 0;
    for (int loopCount = 0; loopCount < totalSize / WRITE_BACK_BURST_LENGTH / SIZE_BY_INT; loopCount++)
    {

        for (int i = 0; i < WRITE_BACK_BURST_LENGTH; i++)
        {
#pragma HLS PIPELINE II=1
            burst_raw tmp;
            read_from_stream(input, tmp);
            addr[(counter << LOG_WRITE_BURST_LEN) + i] = tmp;
        }
        counter ++;

    }
}





#endif /*__FPGA_GLOBAL_MEM_H__ */
