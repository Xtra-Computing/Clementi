#ifndef __RW_TYPE_H__
#define __RW_TYPE_H__


#include "../stream_type.h"

#include "rw_data_struct.h"

#define INVALID_VERTEX (0xFFFFFFFF)

GEN_STRUCT_INTERFACE(assert)
GEN_STRUCT_INTERFACE(assert_mask)

GEN_STRUCT_INTERFACE(adjacent)

GEN_STRUCT_INTERFACE(step_metadata)

GEN_STRUCT_INTERFACE(query_metadata)

GEN_STRUCT_INTERFACE(dummy_step)

GEN_STRUCT_INTERFACE(sample_index)

GEN_STRUCT_INTERFACE(weight_mask)

GEN_STRUCT_INTERFACE(mask_cmd)

GEN_STRUCT_INTERFACE(weight)

GEN_STRUCT_INTERFACE(burst_cmd)

GEN_STRUCT_INTERFACE(sync_id)

GEN_STRUCT_INTERFACE(burst_sync)
GEN_STRUCT_INTERFACE(burst_sync_with_state)
GEN_STRUCT_INTERFACE(burst_sync_padded)

GEN_STRUCT_INTERFACE(vertex_descriptor)
GEN_STRUCT_INTERFACE(vertex_descriptor_ordered)
GEN_STRUCT_INTERFACE(vertex_descriptor_with_state)

GEN_STRUCT_INTERFACE(next_vertex)
GEN_STRUCT_INTERFACE(next_vertex_ordered)

GEN_STRUCT_INTERFACE(rs_mask)

GEN_STRUCT_INTERFACE(path)

GEN_STRUCT_INTERFACE(res_data)


#endif /* __RW_TYPE_H__ */
