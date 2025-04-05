# 4 GS kernels run on 4 FPGAs, each FPGA have one GS kernel.
TARGET = hw
DEVICE = xilinx_u250_gen3x16_xdma_3_1_202020_1
FREQ = 250
TYPE = pr
BUILD_DIR := ./build_dir_$(TARGET)_$(APP)_$(TYPE)
## need mpicxx to compile host code, if not use mpicxx, pls REMOVE the CXX_MPI def !!
CXX_MPI = true
CXXFLAGS += -DCXX_MPI=$(CXX_MPI)

include host/pcg/Makefile
include host/xcl2/Makefile
include host/unit_test_multi_node_gp/Makefile
include host/log/Makefile
include host/cmdparser/Makefile

include src/coalesce/Makefile
include src/src_cache/Makefile
include test/test_duplicate_512/Makefile
include test/test_common/Makefile
include src/data_struct/Makefile
include src/header/Makefile
include src/gather_scatter/Makefile

## add application : pr, wcc or bfs;
ifeq ($(TYPE), bfs)
	include src/bfs/Makefile
else ifeq ($(TYPE), wcc)
	include src/wcc/Makefile
else 
	include src/pr/Makefile
endif

LDFLAGS += -lxrt_coreutil -luuid
VPP_FLAGS += -DLOG_CACHEUPDATEBURST=5
CXXFLAGS += -DTEST_GP=1

PROCESSOR_NUM=4 # use 4 FPGAs for test
SUB_PARTITION_NUM=PROCESSOR_NUM # each FPGA have one GS kernel, total 4 kernels => 4 shards.
PARTITION_SIZE=1048576

CXXFLAGS += -DSUB_PARTITION_NUM=$(SUB_PARTITION_NUM)
CXXFLAGS += -DPARTITION_SIZE=$(PARTITION_SIZE)
CXXFLAGS += -DPROCESSOR_NUM=$(PROCESSOR_NUM)

VPP_FLAGS += -DPARTITION_SIZE=$(PARTITION_SIZE)