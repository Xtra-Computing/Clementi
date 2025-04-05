#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <hls_stream.h>

#include "rw_type.h"
#include "rng_type.h"
#include "test_common.h"

#define SIZE_OF_BURST (2)


extern "C" {
    void test_bs_loader(    int                         size,
                            burst_sync_padded_item_t    *mem,
                            burst_sync_stream_t  &output

                       ) {
#if 0
        for (int i = 0; i < size ; i ++)
        {
            burst_sync_pkg_t pkg;
            pkg.data.ap_member(burst_sync_t, addr) = i * 16;
            pkg.data.ap_member(burst_sync_t, id) = i;
            pkg.data.ap_member(burst_sync_t, dynamic_id) = 0;
            pkg.data.ap_member(burst_sync_t, burst_id) = ((i & (SIZE_OF_BURST - 1)) == 0x1) ? 0x8000 : 0x00;

            pkg.last = ((i) == (size - 1));
            output.write(pkg);
        }
        mem[0] = size;
#else
        for (int i = 0; i < size ; i ++)
        {
            burst_sync_padded_item_t bs_data = mem[i];
            burst_sync_pkg_t pkg;
            pkg.data.ap_member(burst_sync_t, addr)          = bs_data.ap_member(burst_sync_padded_t, addr);
            pkg.data.ap_member(burst_sync_t, id)            = bs_data.ap_member(burst_sync_padded_t, id);
            pkg.data.ap_member(burst_sync_t, dynamic_id)    = bs_data.ap_member(burst_sync_padded_t, dynamic_id);
            pkg.data.ap_member(burst_sync_t, burst_id)      = bs_data.ap_member(burst_sync_padded_t, burst_id);

            pkg.last = ((i) == (size - 1));
            output.write(pkg);
        }
#endif
        //test_mem_loader<burst_sync_stream_t, burst_sync_item_t, burst_sync_pkg_t>(size, mem, output);
    }
}
