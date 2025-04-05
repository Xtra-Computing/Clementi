#ifndef __RW_DATA_STRUCT__
#define __RW_DATA_STRUCT__

typedef struct
{
    uint32_t v;
}  adjacent_t;


typedef struct __attribute__((packed)) {
    uint32_t last_vertex;
    uint32_t curr_vertex;
} assert_t;

typedef struct
{
    uint32_t curr_vertex;
    uint32_t remain_step;
} query_metadata_t;

typedef struct
{
    uint32_t curr_vertex;
    uint32_t remain_step;
    uint16_t state;
    uint16_t ring_state;
    uint32_t inner_state;
    uint32_t assert;
} step_metadata_t;


typedef struct
{
    uint32_t curr_vertex;
    uint32_t remain_step;
    uint16_t state;
    uint16_t ring_state;
    uint32_t inner_state;
} res_data_t;


typedef struct  __attribute__((packed)) {
    uint32_t curr_vertex;
    uint32_t remain_step;
    uint16_t state;
    uint16_t ring_state;
    uint32_t inner_state;
    uint32_t assert;
    uint32_t pad[3];
} dummy_step_t;


typedef struct
{
    uint32_t curr_vertex;
    uint32_t inner_state;
} path_t;


typedef struct
{
    uint32_t start;
    uint32_t size;
} vertex_descriptor_t;

typedef uint32_t sample_index_t;

typedef struct
{
    uint32_t vertex;
} next_vertex_t;

typedef uint16_t  weight_mask_t;

typedef struct
{
    uint32_t start;
    uint32_t end;
} mask_cmd_t;

typedef struct
{
    uint32_t data[16];
} weight_t;


typedef struct
{
    uint64_t data[16];
} scaled_weight_t;


typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint32_t size;
    uint32_t start;
    uint32_t end;
    uint16_t  id;
    uint16_t  burst_id;
    uint16_t  dynamic_id;
    uint16_t  state;
} burst_cmd_t;


typedef struct __attribute__((packed)) {
    uint16_t id;
} sync_id_t;




// TODO offset for sampling
typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint16_t id;
    uint16_t burst_id;
    uint16_t dynamic_id;
    uint16_t state;
} burst_sync_t;


typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint16_t id;
    uint16_t burst_id;
    uint16_t dynamic_id;
    uint16_t state;
    uint32_t assert;
} burst_sync_with_state_t;

typedef struct __attribute__((packed))
{
    uint32_t start;
    uint32_t size;
    uint16_t  state;
} vertex_descriptor_with_state_t;


typedef struct __attribute__((packed))
{
    uint32_t start;
    uint32_t size;
    uint16_t  id;
    uint16_t  burst_id;
    uint16_t  state;
} vertex_descriptor_ordered_t;

typedef struct __attribute__((packed))
{
    uint32_t vertex;
    uint16_t id;
} next_vertex_ordered_t;


typedef struct __attribute__((packed)) {
    uint16_t id;
} rs_mask_t;


typedef struct __attribute__((packed)) {
    uint8_t id;
} assert_mask_t;


typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint16_t id;
    uint16_t burst_id;
    uint16_t dynamic_id;
    uint16_t padding[3];
} burst_sync_padded_t;



#endif /* __RW_DATA_STRUCT__ */
