TARGET = hw
DEVICE = xilinx_u250_gen3x16_xdma_3_1_202020_1
FREQ = 250
TYPE = pr
BUILD_DIR := ./build_dir_$(TARGET)_$(APP)_$(TYPE)

include host/pcg/Makefile
include host/xcl2/Makefile
include host/network/Makefile
include host/single_gas/Makefile
include host/log/Makefile
include host/cmdparser/Makefile

include src/coalesce/Makefile
include src/src_cache/Makefile
include test/test_duplicate_512/Makefile
include test/test_common/Makefile
include src/data_struct/Makefile
include src/header/Makefile
include src/single_gas/Makefile

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
CXXFLAGS += -fopenmp

## config.mk for pagerank application
#apply kernel

HAVE_APPLY_OUTDEG=true

CXXFLAGS += -DHAVE_APPLY_OUTDEG=$(HAVE_APPLY_OUTDEG)
VPP_FLAGS += -DHAVE_APPLY_OUTDEG=$(HAVE_APPLY_OUTDEG)
## parameter configuration
SUB_PARTITION_NUM=4
CXXFLAGS += -DSUB_PARTITION_NUM=$(SUB_PARTITION_NUM)
VPP_FLAGS += -DSUB_PARTITION_NUM=$(SUB_PARTITION_NUM)

KERNEL_NUM=4
PROCESSOR_NUM=1
PARTITION_SIZE=1048576
APPLY_REF_ARRAY_SIZE=1
USE_SCHEDULER=false
USE_APPLY=true
CXXFLAGS += -DUSE_APPLY=$(USE_APPLY)
CXXFLAGS += -DKERNEL_NUM=$(KERNEL_NUM)
CXXFLAGS += -DPROCESSOR_NUM=$(PROCESSOR_NUM)
CXXFLAGS += -DPARTITION_SIZE=$(PARTITION_SIZE)
CXXFLAGS += -DAPPLY_REF_ARRAY_SIZE=$(APPLY_REF_ARRAY_SIZE)
CXXFLAGS += -DUSE_SCHEDULER=$(USE_SCHEDULER)

VPP_FLAGS += -DPARTITION_SIZE=$(PARTITION_SIZE)
