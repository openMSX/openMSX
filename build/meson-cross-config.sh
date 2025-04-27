#!/bin/sh

case $OPENMSX_TARGET_OS in
  mingw*)
    SYSTEM=windows
    ;;
  solaris)
    SYSTEM=sunos
    ;;
  *)
    SYSTEM=$OPENMSX_TARGET_OS
    ;;
esac

CPU=$OPENMSX_TARGET_CPU
ENDIAN=little
case $OPENMSX_TARGET_CPU in
  m68k | mips | ppc | ppc64 | s390 | sparc)
    ENDIAN=big
    ;;
  aarch64_be)
    CPU=aarch64
    ENDIAN=big
    ;;
  avr32)
    CPU=avr
    ENDIAN=big
    ;;
  hppa)
    CPU=parisc
    ENDIAN=big
    ;;
  mipsel)
    CPU=mips
    ;;
  ppc64le)
    CPU=ppc64
    ;;
  sh)
    CPU=sh4
    ;;
  sheb)
    CPU=sh4
    ENDIAN=big
    ;;
esac

cat <<EOF
[binaries]
c = '${CC}'
ar = '${AR}'
ranlib = '${RANLIB}'
strip = '${STRIP}'
pkg-config = '${PKG_CONFIG}'

[host_machine]
system = '${SYSTEM}'
cpu_family = '${CPU}'
cpu = '${CPU}'
endian = '${ENDIAN}'
EOF
