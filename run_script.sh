#!/bin/bash
cd build
make -j4
if [ $? -ne 0 ]; then
    echo "make -j4 failed. Exiting."
    exit 1
fi

cd ..
./build/dramsim3main configs/HBM2_4Gb_x128.ini -f configs/DDR4_8Gb_x8_2666.ini -t ../trace/random_trace_col_4.txt