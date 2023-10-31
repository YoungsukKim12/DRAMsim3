#!/bin/bash

cd build
make -j4
if [ $? -ne 0 ]; then
    echo "make -j4 failed. Exiting."
    exit 1
fi

cd ..

# ./build/dramsim3main configs/HBM2_4Gb_x128.ini -f configs/DDR4_8Gb_x8_2666.ini -t ../dlrm_yskim/traces/HEAM_trace_group_1/random_trace_col_4_vecsize_64.txt

for col_size in 4 ; do
    for vec_size in 64 128 256 512; do
        echo "Running with col size: $col_size and vec size: $vec_size"
        ./build/dramsim3main configs/HBM2_4Gb_x128.ini -f configs/DDR4_8Gb_x8_2666.ini -t ../dlrm_yskim/traces/correct_trace_0/random_trace_col_${col_size}_vecsize_${vec_size}.txt
    done
done