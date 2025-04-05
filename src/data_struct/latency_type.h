#ifndef __LATENCY_TYPE_H__
#define __LATENCY_TYPE_H__

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

#include "../stream_type.h"

#include "latency_data_struct.h"

GEN_STRUCT_INTERFACE(timestamp)
GEN_STRUCT_INTERFACE(stride_addr)
GEN_STRUCT_INTERFACE(dummy_value)
GEN_STRUCT_INTERFACE(time_tag)
GEN_STRUCT_INTERFACE(diff_time)


#endif /* __LATENCY_TYPE_H__ */
