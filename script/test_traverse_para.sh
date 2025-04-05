#!/bin/bash

APP=rw_pwrs_traverse_dyburst
DIR=../test_output_dir

mkdir -p ./${DIR}/build_log/
mkdir -p ./${DIR}/exec_log/

PARA_LIST=( '2' \
            '4' \
            '8' \
            '16' \
            '32' \
            '64' \
)

#cp build_dir_${APP}/kernel.xclbin ${DIR}/kernel.xclbin

#for para_list  in "${PARA_LIST[@]}"

for para_list in  "${PARA_LIST[@]}"
do
  make cleanall
  make app=${APP}  test_size=${para_list} all -j8 2>&1 | tee  ./${DIR}/build_log/build_${para_list}.log

  cp build_dir_${APP}/kernel.xclbin ${DIR}/kernel_${para_list}.xclbin

  #./${APP}.app ${DIR}/kernel.xclbin  | tee ./${DIR}/exec_log/exec_${para_list}.log

  echo "next para"
  #wait

done

