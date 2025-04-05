
include mk/utils.mk
XSA :=
ifneq ($(DEVICE), )
XSA := $(call device2xsa, $(DEVICE))
endif

PYTHON = python3






.PHONY: emconfig connect_info post_compile

post_compile:
	cp -f ${DEFAULT_CFG}.json.pdf  $(BUILD_DIR)/
	@[ -e ${POST_COMPILE_SCRIPT} ] && { \
		echo found post compile script ; \
		./${POST_COMPILE_SCRIPT} ${BUILD_DIR} ;\
	} || { \
		echo Post compile done; \
	}

connect_info: ${DEFAULT_CFG}
	$(info    DEFAULT_CFG is $(DEFAULT_CFG))
	$(PYTHON) ./script/connect_parser.py  ${DEFAULT_CFG}
	cp -f ${DEFAULT_CFG}.json.pdf /data/connect_info/$(APP)_kernel.pdf | true

emconfig:$(EMCONFIG_DIR)/emconfig.json
$(EMCONFIG_DIR)/emconfig.json:
	emconfigutil --platform $(DEVICE) --od $(EMCONFIG_DIR)





.PHONY: all host binary

all:
	$(MAKE) info
	$(MAKE) connect_info
	$(MAKE) check-devices $(EXECUTABLE) $(BINARY_CONTAINERS)
	$(MAKE) emconfig
	$(MAKE) post_compile


host:  info  $(EXECUTABLE)

binary:
	$(MAKE) info
	$(MAKE) connect_info
	$(MAKE) check-devices $(BINARY_CONTAINERS)
	$(MAKE) emconfig
	$(MAKE) post_compile

cleanhost: info
	$(MAKE) cleanobj
	$(MAKE) $(EXECUTABLE)
	@echo "build a clean host"


############################## Cleaning Rules ##############################

.PHONY: clean cleanall cleanfpga cleanhost

clean:
	-$(RMDIR) $(EXECUTABLE) $(XCLBIN)/{*sw_emu*,*hw_emu*}
	-$(RMDIR) profile_* TempConfig system_estimate.xtxt *.rpt *.csv
	-$(RMDIR) src/*.ll *v++* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb
	-$(RMDIR) .run
	-$(RMDIR) *.app
	-$(RMDIR) *.protoinst

cleanall: clean
	-$(RMDIR) build_dir* sd_card*
	-$(RMDIR) package*
	-$(RMDIR) _x* *xclbin.run_summary qemu-memory-_* emulation _vimage pl* start_simulation.sh *.xclbin
	-$(RMDIR) fast_build
cleanfpga:
	-$(RMDIR) build_dir*



cleanobj:
	-$(RMDIR) $(EXECUTABLE)
	-$(RMDIR) $(ALL_OBJECTS)



ifeq (hostemu,$(firstword $(MAKECMDGOALS)))
# use the rest as arguments for "run"
RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
# ...and turn them into do-nothing targets
$(eval $(RUN_ARGS):;@:)
endif


prog: # ...
    # ...

.PHONY: hostemu
hostemu: cleanhost prog
	@echo args: $(RUN_ARGS)
	kill $(shell pidof xsim) | true
	XCL_EMULATION_MODE=hw_emu  catchsegv ./$(EXECUTABLE)  -fpga ${BUILD_DIR}/kernel.xclbin -qsize 512 $(RUN_ARGS)



.PHONY: automation
automation:
	$(shell ./src/automation.sh > generator.log)
