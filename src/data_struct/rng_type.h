#ifndef __RNG_TYPE_H__
#define __RNG_TYPE_H__

#include "../stream_type.h"

#include "rng_data_struct.h"



typedef ap_uint<RNG_WIDTH>      rng32_t;


GEN_STRUCT_INTERFACE(rng_state)

GEN_STRUCT_INTERFACE(rng_output)


#ifndef __SYNTHESIS__
    #define DEBUG_PRINT(f_, ...) printf((f_), __VA_ARGS__);
#else
    #define DEBUG_PRINT(f_, ...) ;
#endif


#endif /* __RNG_TYPE_H__ */
