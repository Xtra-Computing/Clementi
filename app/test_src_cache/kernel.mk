TARGET = hw_emu
DEVICE = xilinx_u250_gen3x16_xdma_3_1_202020_1
FREQ = 300

include host/pcg/Makefile
include host/xcl2/Makefile
include host/helper/Makefile

include test/host_src_cache/Makefile

include src/coalesce/Makefile
include src/src_cache/Makefile

include test/test_common/Makefile
include test/test_store_512/Makefile

include test/test_load_512_with_end_token/Makefile
include test/test_duplicate_512/Makefile

VPP_FLAGS += -DLOG_CACHEUPDATEBURST=3
