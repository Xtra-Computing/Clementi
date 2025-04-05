include mk/phase_init.mk

APP ?= clementi

include mk/phase_app.mk

ifdef CXX_MPI
## need to change to the correct path in your environment
# CXX := /opt/openmpi/bin/mpicxx
CXX := /usr/bin/mpicxx
endif

include mk/phase_misc.mk
include mk/phase_sw.mk
include mk/phase_hw.mk