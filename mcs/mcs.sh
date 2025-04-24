#! /bin/sh
 # This file (mcs.sh) was created by Ron Rechenmacher <ron@fnal.gov> on
 # Jan  3, 2015. "TERMS AND CONDITIONS" governing this file are in the README
 # or COPYING file. If you do not have such a file, one can be obtained by
 # contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
 # $RCSfile: .emacs.gnu,v $
 # rev='$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $'

devnum=$1; shift
file=$1; shift

# find the mcs and devl executables
if type mcs >/dev/null;then :;else
    echo "ERROR - mcs and devl executables not found - setup mu2e_pcie_utils"
    exit
fi

pids=`lsof -t /dev/mu2e* 2>/dev/null` && kill $pids

lsmod | grep pci_devel_main >/dev/null && rmmod pci_devel_main
sleep 1
lsmod | grep mu2e           >/dev/null && rmmod mu2e
lsmod | grep TRACE          >/dev/null && rmmod TRACE

if ! [ -z "${MRB_BUILDDIR-}" ] && [ -e "${MRB_BUILDDIR}/TRACE/module/`uname -r`/TRACE.ko" ]; then
    source $TRACE_DIR/script/trace.sh.functions
else
  # (re)setup TRACE (no get functions defined in this script
    . /mu2e/ups/setup
    test -n "${SETUP_TRACE-}"\
   && { xx=$SETUP_TRACE; unsetup TRACE; eval setup $xx;}\
   || setup TRACE
fi

modprobe TRACE
modprobe pci_devel_main

export TRACE_FILE=/proc/trace/buffer
tonSg 0-7; tonMg 0-15


echo "Detected mu2e devices:"

for dev in /dev/pcidev*;do
	echo $dev
    ii=${dev#/dev/pcidev}
    echo 'reg 0x9000 value...'
    devl $ii uint32 0x9000
    echo 'reg 0x9004 value...'
    devl $ii uint32 0x9004
done

if [ "x$file" == "x" ] || ! [ -e $file ]; then
	echo "Invalid or null file argument!"
	 echo "usage: `basename $0` <devicenum> <file>";
	 exit
fi

export TRACE_NAME=mcs
mcs $devnum $file | tee /tmp/mcs.out\
 |{ xx=11;while xx=`expr $xx - 1`;do IFS= read ln;echo "$ln";done;echo ...;tail;}

echo "Now Power Cycle!!!!!!!!!"
