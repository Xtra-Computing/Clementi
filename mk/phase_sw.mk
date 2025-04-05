############################## Setting up Host Variables ##############################
#Include Required Host Source Files
CXXFLAGS += -I$(XF_PROJ_ROOT)/common/includes/xcl2

# Host compiler global settings
CXXFLAGS += -fmessage-length=0
CXXFLAGS += -I ./src/
LDFLAGS += -lrt -lstdc++ -lxilinxopencl


include $(XF_PROJ_ROOT)/common/includes/opencl/opencl.mk

CXXFLAGS += $(opencl_CXXFLAGS) -Wall -O0 -g -rdynamic -std=c++17
LDFLAGS += $(opencl_LDFLAGS)



#ALL_OBJECTS_PATH:=$(addprefix $(TEMP_DIR)/host/,$(SRC_OBJS))
#ALL_OBJECTS:= $(addsuffix .o, $(ALL_OBJECTS_PATH))

############################## Setting Rules for Host (Building Host Executable) ##############################
$(EXECUTABLE):  check-xrt | $(ALL_OBJECTS)
	@echo ${LIGHT_BLUE}"Start linking files"${NC}
	@echo $(LDFLAGS)
	$(CXX)   $(CXXFLAGS) $(LDFLAGS) $(ALL_OBJECTS) -Xlinker -o$@ $(LDFLAGS) $(LIBS)
	@cp $(EXECUTABLE)  ${BUILD_DIR}/$(EXECUTABLE)
	@echo ${LIGHT_BLUE}"Build done"${NC}


