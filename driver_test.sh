#!/bin/bash

make clean
make

#insmod trace/src_module/TRACE.ko
insmod trace/src_module/TRACE.ko trace_allow_printk=1 trace_lvlS=0xFFFFFF trace_print='"%T %i %n %L %M"'

set -x

for ii in {0..200};do
	echo `date`: rmmod $ii;
	rmmod mu2e;
	journalctl --sync;
	sleep 0.1;
	echo `date`: insmod $ii;
	insmod mu2e_driver/mu2e.ko;
	journalctl --sync;
	sleep 0.1;
done

set +x
