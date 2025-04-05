#!/bin/bash
if [ $# -lt 2 ]; then
  echo "Usage: $0 DatasetName Superstep_Number"
  exit 1
fi

D_name="$1"
S_number="$2"

echo "Run dataset: $D_name , super step number: $S_number"

/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./clementi.app -d $D_name -s $S_number
