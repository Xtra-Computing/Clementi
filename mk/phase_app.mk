FREQ = 300 ## default frequency

EXECUTABLE := $(APP).app
BUILD_DIR := ./build_dir_$(TARGET)_$(APP)
TEMP_DIR := ./_x_$(TARGET)_$(APP)


include src/data_struct/Makefile
include app/$(APP)/kernel.mk

DEFAULT_CFG  ?= app/$(APP)/kernel.cfg
CFG_FILE ?= app/$(APP)/kernel.cfg