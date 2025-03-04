#test `whoami` == root || { echo 'You are not root and must be; script returning.'; return; }
test "$0" == "$BASH_SOURCE" && { echo 'You must source this script; script exiting'; exit; }

MU2EHOST=`hostname|grep -v mu2edaq01|grep -c mu2edaq`
if [ $MU2EHOST -gt 0 ];then
  test -e /dev/mu2e0 || { echo 'DTC device file not found. Please re-run this script as root!'; return; }

  echo "Setting up TRACE module"
  export TRACE_FILE=/proc/trace/buffer;tonM -nKERNEL 0-19;toffM -nKERNEL 4  # poll noise is on lvls 22-23
  tonSg 0
  export DTCLIB_SIM_ENABLE=N
else
  export TRACE_FILE=/tmp/trace_buffer_$USER
  export DTCLIB_SIM_ENABLE=1
fi
#echo "Doing \"Super\" Reset Chants"
#my_cntl write 0x9100 0xa0000000  >/dev/null # reset DTC  reset serdes osc
#my_cntl write 0x9100 0x00000000  >/dev/null  # clear reset
# v3.0 firmware doesn't have SERDES...doing this results in error!
#rocUtil toggle_serdes
#rocUtil reset_roc
#rocUtil write_register -a 14 -w 0x010

TRACE_NAME=MU2EDEV tonM -nMU2EDEV 0-31; tonM 0-31

DTC_Test()
{
    treset;tmodeM 1;DTCLIB_SIM_ENABLE=N mu2eUtil buffer_test -a 0 "$@"
}

DTC_Test2()
{
    treset;tmodeM 1;DTCLIB_SIM_ENABLE=N mu2eUtil buffer_test -a 0 -n 1 -c 50 -S -q -t "$@"
}
DTC_Test3()
{
    treset;tmodeM 1;DTCLIB_SIM_ENABLE=N mu2eUtil buffer_test -a 0 -n 1 -c 200 -S -Q -T 2
}
DTC_ReadFile()
{
    od -x $@|less
}
DTC_Test_ROC_emulation()
{   # sim modes -- see DTC.cc:DTC
    treset;tmodeM 1;DTCLIB_SIM_ENABLE=R mu2eUtil "$@"
}

DTC_Reset()
{
  dtc=-1
  if [ $# -ge 1 ];then
    dtc=$1
  fi
#    my_cntl write 0x9100 0x30000000 >/dev/null;: Oscillator resets;\
#    my_cntl write 0x9100 0x80000000 >/dev/null;: DTC reset, Clear Oscillator resets;\
#    my_cntl write 0x9100 0x00000000 >/dev/null;: Clear DTC reset;\
#    my_cntl write 0x9118 0x0000003f >/dev/null;: Reset all links;\
#    my_cntl write 0x9118 0x00000000 >/dev/null;: Clear Link Resets

  my_cntl -d $dtc write 0x9100 0x80000000  >/dev/null # soft reset DTC
  my_cntl -d $dtc write 0x9100 0x00000000  >/dev/null # unreset DTC
#   my_cntl -d $dtc write 0x9100 0x00008000 > /dev/null # Turn on CFO Emulation Mode for Serdes Reset
#   my_cntl -d $dtc write 0x9118 0xffff00ff  >/dev/null  # SERDES resets
#   my_cntl -d $dtc write 0x9118 0x00000000  >/dev/null  # clear SERDES reset on link 0

#   sleep 1


  echo "SERDES Reset Done after reset: "
#   my_cntl -d $dtc read 0x9138
}

ROC_Reset()
{
    rocUtil toggle_serdes
    rocUtil reset_roc
    rocUtil write_register -a 14 -w 0x010
}

DTC_TestDDR() {
     treset;
     tonM1;
     DTCLIB_SIM_ENABLE=N mu2eUtil buffer_test -a 0 -n 1 -c 10 -S -q -T 4
 }

DTC_TestSRAM() { 
    treset;
    tonM1;
    DTCLIB_SIM_ENABLE=N mu2eUtil buffer_test -a 0 -n 1 -c 10 -S -q -T 3 
}

DTC_Links() { 
    DTCRegDump|grep -A1 "RX Buffer Status" 
}

DTC_LoadData() {
    treset;
    tonM1;
    DTCLIB_SIM_ENABLE=N mu2eUtil -n 4 -c 10 -g 4 -G buffer_test
}

DTC_ReadLoadedData() {
    treset;
    tonM1;
    DTCLIB_SIM_ENABLE=N mu2eUtil -n 4 -G buffer_test
}

echo '\nIf there were no errors, you should now be able to test with: DTC_Test [NumReqs]'
echo '       or DTC_Test_ROC_emulation [NumReqs]'
echo 'Example: DTC_Test -n 1       # send       1 read-out/data request pair'
echo '         DTC_Test -n 1000000 -f /tmp/mu2etest.bin # send 1000000 read-out/data request pair, log binary data to /tmp/mu2etest.bin'
echo '         DTC_Test2 -f /tmp/mu2etest.bin # Same as DTC_Test -n 1 -q -c 50 -t -f /tmp/mu2etest.bin'
echo '         DTC_Test3 -f /tmp/mu2eraw.bin # Same as DTC_Test -n 1 -c 200 -S -Q -T 2 -f /tmp/mu2eraw.bin'
echo 'Use DTC_TestDDR and DTC_TestSRAM for RAM Error Checking modes'
echo 'Use DTC_Links to see which of Link0/1 are currently connected'
echo 'and reset with DTC_Reset and ROC_Reset'
echo 'Use DTC_LoadData to generate data, send it to the DTC and print the generated buffers to screen'
echo 'Use DTC_ReadLoadedData to read data generated using DTC_LoadData back'
echo 'Use "DTC_Test -h" to see other options.'
echo
echo 'To see the current state of all of the DTC Registers, use DTCRegDump'
echo
echo 'To view a file saved with DTC_Test -f, use DTC_ReadFile <filename>'
echo
