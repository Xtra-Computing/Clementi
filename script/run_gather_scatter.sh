#!/bin/bash

# Please check the PATH of mpirun
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 4 --hostfile ./host_file ./gather_scatter.app -d R24