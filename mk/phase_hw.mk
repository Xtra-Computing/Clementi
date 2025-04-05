############################## Setting up Kernel Variables ##############################
# Kernel compiler global settings
VPP_FLAGS += -t $(TARGET) --platform $(DEVICE) --save-temps
ifneq ($(TARGET), hw)
	VPP_FLAGS += -g
endif

VPP_FLAGS += -I ./src/ --vivado.param general.maxThreads=32 --vivado.synth.jobs 32

# Kernel linker flags
VPP_LDFLAGS += --kernel_frequency $(FREQ)
VPP_LDFLAGS += --vivado.param general.maxThreads=32  --vivado.impl.jobs 32 --config $(CFG_FILE)



############################## Setting Rules for Binary Containers (Building Kernels) ##############################
$(BUILD_DIR)/kernel.xclbin:  $(BINARY_CONTAINER_OBJS) $(KERNEL_HOLDER_BINARY_CONTAINER_OBJS)
	@echo $(BINARY_CONTAINER_OBJS)
ifeq ($(HOST_ARCH), x86)
	$(VPP) $(VPP_FLAGS) -l $(VPP_LDFLAGS) --temp_dir $(TEMP_DIR)  -o'$(BUILD_DIR)/kernel.link.xclbin' $(BINARY_CONTAINER_OBJS) $(KERNEL_HOLDER_BINARY_CONTAINER_OBJS)
	$(VPP) -p $(BUILD_DIR)/kernel.link.xclbin -t $(TARGET) --platform $(DEVICE) --package.out_dir $(PACKAGE_OUT) -o $(BUILD_DIR)/kernel.xclbin
else
	$(VPP) $(VPP_FLAGS) -l $(VPP_LDFLAGS) --temp_dir $(TEMP_DIR) -o'$(BUILD_DIR)/kernel.xclbin' $(+)
endif
	cp $(TEMP_DIR)/reports/link/imp/impl_1_full_util_routed.rpt    ${BUILD_DIR}/report/  | true
	cp $(TEMP_DIR)/reports/link/imp/impl_1_kernel_util_routed.rpt  ${BUILD_DIR}/report/  | true
	cat $(TEMP_DIR)/link/vivado/vpl/runme.log |  grep scaled\ frequency >   ${BUILD_DIR}/clock.log | true



.PHONY: build
build: check-vitis  $(BINARY_CONTAINERS)

.PHONY: xclbin
xclbin: build

.PHONY: reset
reset:
	(sleep  1 &&  echo -e -n "y\n\n") | xbutil reset
	echo "done"
