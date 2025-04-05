#ifndef __GP_DATA_STRUCT__
#define __GP_DATA_STRUCT__

typedef struct __attribute__((packed))
{
    uint32_t data[16];
} source_vertex_t;


typedef struct __attribute__((packed))
{
    uint32_t data[4];
} quad_source_vertex_t;

typedef struct __attribute__((packed))
{
    uint32_t data;
} single_source_vertex_t;

typedef struct __attribute__((packed))
{
    uint32_t idx;
    uint32_t size;
} cache_command_t;

typedef struct __attribute__((packed))
{
    uint32_t data[16];
    uint32_t addr;
} cache_line_t;



#endif /* __GP_DATA_STRUCT__ */
