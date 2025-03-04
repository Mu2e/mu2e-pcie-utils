#! /bin/sh
 # This file (switch_test.sh) was created by Ron Rechenmacher <ron@fnal.gov> on
 # Mar  8, 2017. "TERMS AND CONDITIONS" governing this file are in the README
 # or COPYING file. If you do not have such a file, one can be obtained by
 # contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
 # $RCSfile: .emacs.gnu,v $
 # rev='$Revision: 1.30 $$Date: 2016/03/01 14:27:27 $'
script=`basename $0`
USAGE="\
   usage: $script [opts] <cmd>...
 options:
-p<pkts>
-b<bytes>   # Payload - must be 8*x+2  and min 50. default 74.  i.e. 74,82,1442
            # Currently all pkts same. FW does support different sizes.
            # Biggest size would be added to gap (internally) to make
            # pkt interval
-g<gaps>    # 6.20606 ns clocks    default 52
-d<numdsts> # includes yourself (min 1)
-c<chan>    # FOR read_counters|read
examples:

$script init

: ER; $script -p10000 -g50 -b1442 reset_counters addr dest num_dst enable G read
: OK; $script -p10000 -g50 -b538 reset_counters addr dest num_dst enable G read
: ER; $script -p10000 -g50 -b642 reset_counters addr dest num_dst enable G read
: OK; $script -p5000 -g50 -b642 reset_counters addr dest num_dst enable G read
: OK; $script -p5000 -g50 -b1442 reset_counters addr dest num_dst enable G read
: ER; $script -p6000 -g50 -b1442 reset_counters addr dest num_dst enable G read
: OK; $script -p5500 -g50 -b1442 reset_counters addr dest num_dst enable G read
: intermittent; $script -p5600 -g50 -b1442 reset_counters addr dest num_dst enable G read
: INTERMITTENT; $script -p5550 -g50 -b1442 reset_counters addr dest num_dst enable G read
: ER; $script -p100000 -g50 -b322 reset_counters addr dest num_dst enable G read
: ER; $script -p100000 -g50 -b74 reset_counters addr dest num_dst enable G read
: ER; $script -p100000 -g 9 -b74 reset_counters addr dest num_dst enable G read
: ER; $script -p100000 -g10 -b74 reset_counters addr dest num_dst enable G read
: ER; $script -p100000 -g 8 -b74 reset_counters addr dest num_dst enable G read
: ER; $script -p 33000 -g 9 -b74 reset_counters addr dest num_dst enable G read
: OK; $script -p 30000 -g 9 -b74 reset_counters addr dest num_dst enable G sleep read
: ??; $script -p 20000 -g150 -b642 reset_all_counters addr dest num_dst enable G read_all_counters -d 2

          $script soft
commands:
`grep '^ *[a-zA-Z_|]*)' $0`
"

opt_pkts=1
opt_bytes=74
opt_gap=0
opt_dests=4
opt_channel=0

op1chr='rest=`expr "$op" : "[^-]\(.*\)"`; test -n "$rest" && set -- "-$rest" "$@"'
op1arg='rest=`expr "$op" : "[^-]\(.*\)"`; test -n "$rest" && set -- "$rest"  "$@"'
reqarg="$op1arg;"'test -z "${1+1}" &&echo opt -$op requires arg. &&echo "$USAGE" &&exit'
args= do_help= opt_v=0
while [ -n "${1-}" ];do
    if expr "x${1-}" : 'x-' >/dev/null;then
        op=`expr "x$1" : 'x-\(.*\)'`; shift   # done with $1
        leq=`expr "x$op" : 'x-[^=]*\(=\)'` lev=`expr "x$op" : 'x-[^=]*=\(.*\)'`
        test -n "$leq"&&eval "set -- \"\$lev\" \"\$@\""&&op=`expr "x$op" : 'x\([^=]*\)'`
        case "$op" in
        \?*|h*)     eval $op1chr; do_help=1;;
        v*)         eval $op1chr; opt_v=`expr $opt_v + 1`;;
        x*)         eval $op1chr; test $opt_v -ge 1 && set -xv || set -x;;
        p*)         eval $op1arg; opt_pkts=$1;     shift;;
        g*)         eval $op1arg; opt_gap=$1;      shift;;
        b*)         eval $op1arg; opt_bytes=$1;    shift;;
        d*)         eval $op1arg; opt_dests=$1;    shift;;
        c*)         eval $op1arg; opt_channel=$1;  shift;;
        esac
    else # allow mix of opts and arg (i.e. opts after args)
        aa=`echo "$1" | sed -e "s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa
set -u

if [ $opt_dests -lt 1 ] || [ $opt_dests -gt 6 ];then
  echo "Invalid number of destinations! Must be between 1 and 6!";
  exit 1;
fi

test $# -eq 0 && { echo "$USAGE"; exit; }

cmds=$*

calcf() { fmt=$1;shift;awk "BEGIN{printf \"$fmt\", $*;quit}"; }

while [ -n "${1-}" ];do
  cmd=$1; shift
  case $cmd in
  reset_serdes|serdes_reset)
    #disable links
    my_cntl write 0x9114 0x00000000

    #SERDES reset
    my_cntl write 0x9118 0x000000ff
    my_cntl write 0x9118 0x00000000
    sleep .2
    ;;

  init|soft|reset)
    #disable links
    my_cntl write 0x9114 0x00000000

    #turn on lasers and set rate select
    my_cntl write 0x9110 0x0000ff00

    #softreset
    my_cntl write 0x9100 0x80000000
    my_cntl write 0x9100 0x00000000
    sleep .1
    ;;

read_all)
echo "System Registers"
echo -n '                         0x9100: '; my_cntl read 0x9100 | grep ^0x
echo -n 'rate sel                 0x9110: '; my_cntl read 0x9110 | grep ^0x
echo -n 'link/port enable         0x9114: '; my_cntl read 0x9114 | grep ^0x
echo -n 'serdes reset             0x9118: '; my_cntl read 0x9118 | grep ^0x
echo -n 'SERDES Unlock Error      0x9124: '; my_cntl read 0x9124 | grep ^0x
echo -n 'SERDES PLL Locked        0x9128: '; my_cntl read 0x9128 | grep ^0x
echo -n 'SFP LightLoss/NotPresent 0x912c: '; my_cntl read 0x912c | grep ^0x
echo -n 'SERDES RX Idle Chars     0x9140: '; my_cntl read 0x9140 | grep ^0x
echo -n 'RX Error                 0x91f0: '; my_cntl read 0x91f0 | grep ^0x

echo "port 0 addr: `my_cntl read 0x9170 | grep ^0x` `my_cntl read 0x9174 | grep ^0x`"
echo "port 1 addr: `my_cntl read 0x9178 | grep ^0x` `my_cntl read 0x917c | grep ^0x`"
echo "port 2 addr: `my_cntl read 0x9180 | grep ^0x` `my_cntl read 0x9184 | grep ^0x`"
echo "port 3 addr: `my_cntl read 0x9188 | grep ^0x` `my_cntl read 0x918c | grep ^0x`"
echo "port 4 addr: `my_cntl read 0x9190 | grep ^0x` `my_cntl read 0x9194 | grep ^0x`"
echo "port 5 addr: `my_cntl read 0x9198 | grep ^0x` `my_cntl read 0x919c | grep ^0x`"
echo "port 0 dest: `my_cntl read 0x91b0 | grep ^0x` `my_cntl read 0x91b4 | grep ^0x`"
echo "port 1 dest: `my_cntl read 0x91b8 | grep ^0x` `my_cntl read 0x91bc | grep ^0x`"
echo "port 2 dest: `my_cntl read 0x91c0 | grep ^0x` `my_cntl read 0x91c4 | grep ^0x`"
echo "port 3 dest: `my_cntl read 0x91c8 | grep ^0x` `my_cntl read 0x91cc | grep ^0x`"
echo "port 4 dest: `my_cntl read 0x91d0 | grep ^0x` `my_cntl read 0x91d4 | grep ^0x`"
echo "port 5 dest: `my_cntl read 0x91d8 | grep ^0x` `my_cntl read 0x91dc | grep ^0x`"
#set number of destination nodes
echo -n "num_dests                    ";my_cntl read 0x9160 | grep ^0x
echo -n "packet interval              ";my_cntl read 0x9164 | grep ^0x
echo -n "packet sizes for 1st 2 ports ";my_cntl read 0x9150 | grep ^0x
echo -n "packet sizes for 2nd 2 ports ";my_cntl read 0x9154 | grep ^0x
echo -n "packet sizes for 3rd 2 ports ";my_cntl read 0x9158 | grep ^0x
echo -n "number of packets to tx      ";my_cntl read 0x9168 | grep ^0x
;;

addr)
# port 0 (top left)
my_cntl write 0x9170 0xa0000000
my_cntl write 0x9174 0x00000000
# port 1 (left, 2nd from top)
my_cntl write 0x9178 0xa0000001
my_cntl write 0x917c 0x00000000
# port 2 (left, 3rd (from top))
my_cntl write 0x9180 0xa0000002
my_cntl write 0x9184 0x00000000
# port 3 (left, 4th (from top))
my_cntl write 0x9188 0xa0000003
my_cntl write 0x918c 0x00000000
# port 4 (top right)
my_cntl write 0x9190 0xa0000004
my_cntl write 0x9194 0x00000000
# port 5 (right, 2nd (from top))
my_cntl write 0x9198 0xa0000005
my_cntl write 0x919c 0x00000000
;;

  dest)
    # 1st dest for port 0
    my_cntl write 0x91b0 0xa0000001
    my_cntl write 0x91b4 0x00000000
    test $opt_dests -le 1 && continue
    # 1st dest for port 1
    my_cntl write 0x91b8 $((0xa0000000 + 2 % $opt_dests))
    my_cntl write 0x91bc 0x00000000
    test $opt_dests -le 2 && continue
    # 1st dest for port 2
    my_cntl write 0x91c0 $((0xa0000000 + 3 % $opt_dests))
    my_cntl write 0x91c4 0x00000000
    test $opt_dests -le 3 && continue
    # 1st dest for port 3
    my_cntl write 0x91c8 $((0xa0000000 + 4 % $opt_dests))
    my_cntl write 0x91cc 0x00000000
    test $opt_dests -le 4 && continue
    # 1st dest for port 4
    my_cntl write 0x91d0 $((0xa0000000 + 5 % $opt_dests))
    my_cntl write 0x91d4 0x00000000
    test $opt_dests -le 5 && continue
    # 1st dest for port 5
    my_cntl write 0x91d8 $((0xa0000000 + 6 % $opt_dests))
    my_cntl write 0x91dc 0x00000000
    ;;

  num_dst)
    #set number of destination nodes
    my_cntl write 0x9160 $opt_dests
    #set packet interval
    pkt_clks=`calcf "%d" "($opt_bytes+14)/8+15"` # +25
    pkt_time=`calcf "%f" $pkt_clks \* 6.20606` # ns
    intv_clks=`calcf "%d" $pkt_time / 4`
    interval=`calcf "%d" $intv_clks + $opt_gap`
    echo interval=$interval
    my_cntl write 0x9164 $interval
    #set packet sizes for 1st 2 ports
    my_cntl write 0x9150 `calcf "0x%08x" $opt_bytes*65536+$opt_bytes`
    #set packet sizes for 2nd 2 ports
    my_cntl write 0x9154 `calcf "0x%08x" $opt_bytes*65536+$opt_bytes`
    #set packet sizes for 3rd 2 ports
    my_cntl write 0x9158 `calcf "0x%08x" $opt_bytes*65536+$opt_bytes`

    #set number of packets to tx
    my_cntl write 0x9168 $opt_pkts
    echo total test time: `calcf "%f" $opt_pkts \* $interval \* 4 / 1000.0` us
    ;;


  enable)
    #set link enables
    msk=`calcf "0x%x" "lshift(1,$opt_dests)-1"`
    my_cntl write 0x9114 $msk >/dev/null
    ;;

  preset)
    #preset dest addresses
    my_cntl write 0x9100 0x00000002
    my_cntl write 0x9100 0x00000000
    ;;


  Go|G)
    #preset dest addresses
    my_cntl write 0x9100 0x00000002 >/dev/null
    my_cntl write 0x9100 0x00000000 >/dev/null
    #tx data enable  --without timestamp reset
    my_cntl write 0x9100 0x00000000 >/dev/null
    my_cntl write 0x9100 0x00000001 >/dev/null
    #my_cntl write 0x9100 0x00000000 >/dev/null
    #sleep `expr $opt_pkts / 10000`
    ;;


  stop|S)
    my_cntl write 0x9100 0x00000000 >/dev/null
   ;;

  read_counters|read)
    chreg=$(( $opt_channel * 4 ))
    rxb=`my_cntl read $(( 0x9200 + $chreg )) | grep ^0x`
    rxp=`my_cntl read $(( 0x9220 + $chreg )) | grep ^0x`
    txb=`my_cntl read $(( 0x9240 + $chreg )) | grep ^0x`
    txp=`my_cntl read $(( 0x9260 + $chreg )) | grep ^0x`
    printf "tx packets:      0x%08x (%7d)\n" $txp $txp
    printf "rx packets:      0x%08x (%7d)\n" $rxp $rxp
    echo -n "tx bytes:        "; echo $txb
    echo -n "rx bytes:        "; echo $rxb
    #read min/max/total/ignored counters
    min_dly=`my_cntl read $(( 0x92c0 + $chreg )) | grep ^0x`
    max_dly=`my_cntl read $(( 0x92e0 + $chreg )) | grep ^0x`
    tot_dly=`my_cntl read $(( 0x9300 + $chreg )) | grep ^0x`
    min_ns=`awk "BEGIN{print 4.0*$min_dly;exit}"`
    max_ns=`awk "BEGIN{print 4.0*$max_dly;exit}"`
    tot_ns=`awk "BEGIN{print 4.0*$tot_dly;exit}"`
    echo "min delay:       $min_ns ns"
    echo "max delay:       $max_ns ns"
    echo "total delay:     $tot_ns ns"
    echo -n "ignored packets: "; my_cntl read $(( 0x9320 + $chreg )) | grep ^0x
    ;;

  read_all_counters)
    prop=('|' '/' '-' '\\' '|' '/' '-' '\\')
    ii=0
    # check for channel 0 pktcnt stable
    rxprv=`my_cntl read 0x9220 | grep ^0x`; sleep .5
    rxp=`my_cntl   read 0x9220 | grep ^0x`
    until [ $rxprv = $rxp ];do
        printf "\r%c - %10d" ${prop[$(($ii&7))]} $rxp
        ii=`expr $ii + 1`
        rxprv=$rxp
        sleep .5
        rxp=`my_cntl   read 0x9220 | grep ^0x`
    done
    printf "\n"
    my_cntl read 0x91f0
    end=`expr $opt_dests - 1`
    for cc in `seq 0 $end`;do
        $0 read_counters -c$cc
    done
    ;;

  reset_counters|reset_c)
    chreg=$(( $opt_channel * 4 ))
    #reset tx/rx packets/bytes counters
    my_cntl write $(( 0x9200 + $chreg )) 0x0
    my_cntl write $(( 0x9220 + $chreg )) 0x0
    my_cntl write $(( 0x9240 + $chreg )) 0x0
    my_cntl write $(( 0x9260 + $chreg )) 0x0
    #reset min/max/total/ignored counters
    my_cntl write $(( 0x92c0 + $chreg )) 0x00000001
    my_cntl write $(( 0x92e0 + $chreg )) 0x00000001
    my_cntl write $(( 0x9300 + $chreg )) 0x00000001
    my_cntl write $(( 0x9320 + $chreg )) 0x00000001
    ;;

  reset_all_counters)
    end=`expr $opt_dests - 1`
    for cc in `seq 0 $end`;do
        $0 reset_counters -c$cc
    done
    ;;

  sleep)  # next param is sleep time
    time=$1; shfit
    sleep $time
    ;;
  *) echo "I'm sorry, Dave, but I can't do that";;
  esac
done

