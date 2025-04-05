#!/bin/bash

APP=test_dyburst
DIR=test_dir

mkdir -p ./${DIR}/build_log/
mkdir -p ./${DIR}/exec_log/

PARA_LIST=( '1' \
            '2' \
            '4' \
            '8' \
            '16' \
            '32' \
            '64' \
)

cp build_dir_${APP}/kernel.xclbin ${DIR}/kernel.xclbin

#for para_list  in "${PARA_LIST[@]}"

for para_list in {1..40}
do
  script -c "make app=${APP}  test_size=${para_list} cleanhost 2>&1"   ./${DIR}/build_log/build_${para_list}.log

  ./${APP}.app ${DIR}/kernel.xclbin  | tee ./${DIR}/exec_log/exec_${para_list}.log

  echo "next para"
  #wait

done

