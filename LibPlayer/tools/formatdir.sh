#!/bin/sh
#*.c
#*.h
dirs=$1
cmd=$0
ddd=${cmd%/*}
fmt=$ddd/format.sh
echo fmtcmd=[$fmt]
formatdir()
{
 dd=$1
 echo try format dir [$dd]
 $fmt $dd/*.c
 $fmt $dd/*.h
 for f in $(dir -d $dd/* );
 do

  if [ -d $f ];then
     formatdir $f
  fi
 done
}
formatdir $dirs
