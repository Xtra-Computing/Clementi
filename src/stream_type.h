#ifndef __STREAM_TYPE__
#define __STREAM_TYPE__

#include "stdint.h"
#include "stddef.h"
#include <ap_int.h>
#include <ap_axi_sdata.h>

#ifdef offset_of

#error "redefined offset_of !"

#else

#define offset_of(st, m)            ((const size_t)((char *)&((st *)0)->m - (char *)0))
#define member_size(type, member)   ((const size_t)(sizeof(((type *)0)->member)))
#define member_dim(type, member)    ((const size_t)(sizeof(((type*)0)->member) / \
                                    sizeof(((type*)0)->member[0])))

#endif

/* Convert a C struct to HLS struct to be used in stream interface */
/*
example:

1. Assuming we have the following struct need to be using in hls stream:

typedef struct
{
    uint32_t a;
    uint16_t b[10];
} test_struct_t;

2. Using the GEN_STRUCT_INTERFACE macro function to add the type that will be used in hls code

GEN_STRUCT_INTERFACE(test_struct)

// Following the rules that type name should be end with "_t", in this macro function, we use "token concatenation" feature,
// hence, the input is "test_struct" rather than "test_struct_t"

// In this example, this macro function redefine "ap_uint<192>" to a new type, named as "test_struct_item_t" which can be
// easily used in HLS code, and the size "192" is automatically calculated from the input C structure.

3. Accessing the member of the HLS item

    3.a access the single member:

        test_struct_item_t hls_var;
        hls_var.ap_member(test_struct_t, a) = 10;

        // this code assign 10 following the bit position of "a", which is described by the C struct "test_struct_t"

    3.b access one of the array member:

        test_struct_item_t hls_var;
        hls_var.ap_member_array(test_struct_t, b,2) = 16;

        // this code assign 16 to the thrid (index 2) elements of "b" in C struct "test_struct_t"

4. Existing problem:
    * Do not use bit domain structure!
    * The alignment should be considerd when declare the C struct (typedef struct __attribute__((packed)) { xx } xx_t;)
    * The size of item that directly loading from memory must be 2^n.

*/

#define member_start_bit(st,m)          (offset_of(st,m) * 8)
#define member_end_bit(st,m)            ((offset_of(st,m) + member_size(st,m)) * 8 - 1)
#define ap_member(st,m)                 range(member_end_bit(st,m), member_start_bit(st,m))
#define ap_member_offset(st,m,o,s)      range(member_start_bit(st,m) + (o + s) - 1, member_start_bit(st,m) + o)

#define member_unit_size(st,m)          ((const size_t)(member_size(st,m)/member_dim(st,m)))
#define member_array_start_bit(st,m,i)  ((const size_t)(member_start_bit(st,m) + (member_unit_size(st,m) * (i) * 8)))
#define member_array_end_bit(st,m,i)    ((const size_t)(member_start_bit(st,m) + (member_unit_size(st,m) * ((i) + 1) * 8) - 1))
#define ap_member_array(st,m,i)         range(member_array_end_bit(st,m,i), member_array_start_bit(st,m,i))

#define ap_frp_last(st)                 range(sizeof(st) * 8, sizeof(st) * 8)
#define ap_frp_data(st)                 range(sizeof(st) * 8  -1 , 0)

#define GEN_STRUCT_INTERFACE(name)                                                  \
typedef ap_uint<(sizeof(name##_t) * 8)>             name##_item_t;                  \
typedef hls::stream< name##_item_t >                name##_inner_stream_t;          \
typedef ap_axiu<(sizeof(name##_t) * 8), 0, 0, 0>    name##_pkg_t;                   \
typedef hls::stream< name##_pkg_t >                 name##_stream_t;                \
typedef ap_uint<(sizeof(name##_t) * 8) + 1>         name##_frp_item_t;              \
typedef hls::stream< name##_frp_item_t >            name##_frp_inner_stream_t;

typedef struct __attribute__((packed))
{
    uint8_t cmd;
} ctrl_t;

GEN_STRUCT_INTERFACE(ctrl)


#define CTRL_END_OF_STREAM                  (1u)



#endif /* __STREAM_TYPE__ */
