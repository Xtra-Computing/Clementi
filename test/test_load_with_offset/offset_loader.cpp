#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"

#define SIZE_OF_BURST (2)


extern "C" {
    void offset_loader(     int                                     size,
                            uint32_t                                *mem,
                            hls::stream< ap_axiu< 32, 0, 0, 0> >    &output,
                            uint64_t                                offset
                       ) {

        for (uint64_t i = 0; i < size ; i ++)
        {
            uint64_t addr = i + offset;
            uint32_t load_data = mem[addr];
            ap_axiu< 32, 0, 0, 0> pkg;
            pkg.data= load_data;
            pkg.last = ((i) == (size - 1));
            output.write(pkg);
        }

    }
}
