TARGET = hw_emu
DEVICE = xilinx_u250_gen3x16_xdma_3_1_202020_1
FREQ = 300


include host/graph_play_ground/Makefile

include host/pcg/Makefile
include host/xcl2/Makefile
include host/helper/Makefile
include host/graph/Makefile
include host/accelerator/Makefile

#include test/test_burst_stream_sink/Makefile
