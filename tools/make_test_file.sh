#!/bin/bash

# DTC Config
export DTCLIB_SIM_ENABLE=E
export DTCLIB_NUM_TRACKER_BLOCKS=270
export DTCLIB_NUM_CALORIMETER_BLOCKS=134
export DTCLIB_NUM_CALORIMETER_HITS=10
export DTCLIB_NUM_CRV_BLOCKS=0

# Create file
mu2eUtil -Q -n 10000 --binary-file-mode DTC_packets.bin buffer_test
