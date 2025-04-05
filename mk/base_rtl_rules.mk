
APP_DIR = $(subdir)

APP_SRC = $(wildcard $(UPPER_DIR)/$(APP_DIR)/*.xo)
APP_SRC_RMDIR = $(patsubst $(UPPER_DIR)/$(APP_DIR)/%, %,$(APP_SRC))
APP_OBJS = $(patsubst %.xo, %,$(APP_SRC_RMDIR))
APP_BINARY_CONTAINERS = $(patsubst %, $(TEMP_DIR)/$(UPPER_DIR)/$(APP_DIR)/%.xo,$(APP_OBJS))

BINARY_CONTAINER_OBJS += $(APP_BINARY_CONTAINERS)
KERNEL_OBJS +=  $(APP_OBJS)
VPP_FLAGS +=  -I $(UPPER_DIR)/$(APP_DIR)

SET_RTL_APP_DIR = $(eval COMPILE_APP_DIR=$(dir $<))
SET_RTL_KERNEL_NAME = $(eval KERNEL_NAME=$(patsubst $(dir $<)%.xo,%, $<))



$(TEMP_DIR)/$(UPPER_DIR)/$(APP_DIR)/%.xo: $(UPPER_DIR)/$(APP_DIR)/%.xo
	$(SET_RTL_APP_DIR)
	$(SET_RTL_KERNEL_NAME)
	@echo  -e  ${BLUE} RTL $(KERNEL_NAME) in $(COMPILE_APP_DIR)${NC}
	mkdir -p $(TEMP_DIR)/$(COMPILE_APP_DIR)

	cp $< $@



unexport APP_SRC
unexport APP_OBJS
unexport APP_DIR
unexport APP_SRC_RMDIR
unexport APP_BINARY_CONTAINERS

