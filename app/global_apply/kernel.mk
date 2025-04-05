# each FPGA have a global_apply kernel
TARGET = hw
DEVICE = xilinx_u250_gen3x16_xdma_3_1_202020_1
FREQ = 250
## need mpicxx to compile host code, if not use mpicxx, pls REMOVE the CXX_MPI def !!
CXX_MPI = true
CXXFLAGS += -DCXX_MPI=$(CXX_MPI)
BUILD_DIR := ./build_dir_$(TARGET)_$(APP)

include host/pcg/Makefile
include host/xcl2/Makefile
include host/log/Makefile
include host/cmdparser/Makefile
include host/global_apply/Makefile
include host/network/Makefile

include src/global_apply/Makefile

## for network stack makefile

INTERFACE := 0 ## use interface 0 and 1
NETLAYERDIR = src/NetLayers/
CMACDIR     = src/Ethernet/
NETLAYERHLS = 100G-fpga-network-stack-core
TEMP_NET_DIR := _x
BINARY_CONTAINER_OBJS += $(NETLAYERDIR)$(TEMP_NET_DIR)/networklayer.xo
VPP_FLAGS += --advanced.param compiler.userPostSysLinkOverlayTcl=$(CMACDIR)post_sys_link.tcl

# enable two interface 0 and 1
BINARY_CONTAINER_OBJS += $(CMACDIR)$(TEMP_NET_DIR)/cmac_0.xo

ifeq (u5,$(findstring u5, $(DEVICE)))
	HLS_IP_FOLDER  = $(shell readlink -f ./$(NETLAYERDIR)$(NETLAYERHLS)/synthesis_results_HBM)
else ifeq (u280,$(findstring u280, $(DEVICE)))
	HLS_IP_FOLDER  = $(shell readlink -f ./$(NETLAYERDIR)$(NETLAYERHLS)/synthesis_results_HBM)
else ifeq (u2,$(findstring u2, $(DEVICE)))
	HLS_IP_FOLDER  = $(shell readlink -f ./$(NETLAYERDIR)$(NETLAYERHLS)/synthesis_results_noHBM)
endif

VPP_LDFLAGS = --user_ip_repo_paths $(HLS_IP_FOLDER)

$(CMACDIR)$(TEMP_NET_DIR)/cmac_0.xo:
	make -C $(CMACDIR) all DEVICE=$(DEVICE) INTERFACE=$(INTERFACE)

$(NETLAYERDIR)$(TEMP_NET_DIR)/networklayer.xo:
	make -C $(NETLAYERDIR) all DEVICE=$(DEVICE)

## for XRT compile
LDFLAGS += -lxrt_coreutil -luuid
CXXFLAGS += -fopenmp

## for FPGA number, take 2 FPGAs as an example
PROCESSOR_NUM=2
CXXFLAGS += -DPROCESSOR_NUM=$(PROCESSOR_NUM)