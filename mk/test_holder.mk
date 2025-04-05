UPPER_DIR := test

mkfile_path := $(abspath $(lastword $(filter-out $(lastword $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
subdir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))

include  mk/kernel_holder_rules.mk

unexport subdir
