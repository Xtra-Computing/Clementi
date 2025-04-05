#ifndef __LATENCY_DATA_STRUCT__
#define __LATENCY_DATA_STRUCT__

typedef struct __attribute__((packed)){
    uint64_t counter;
} timestamp_t;


typedef struct __attribute__((packed)){
    uint32_t addr;
} stride_addr_t;

typedef struct __attribute__((packed)){
    uint32_t value;
} dummy_value_t;


typedef struct __attribute__((packed)){
    uint8_t id;
} time_tag_t;

typedef struct __attribute__((packed)){
    uint64_t cycle;
    uint32_t id;
} diff_time_t;



#endif /* __LATENCY_DATA_STRUCT__ */
