#!/bin/sh

CXXFLAGS='-mthreads -mms-bitfields -Os -mpreferred-stack-boundary=4 -mcpu=pentium3 -march=pentium-mmx -mmmx -fno-force-mem -fno-force-addr -fstrength-reduce -fexpensive-optimizations -fschedule-insns2 -fomit-frame-pointer -fno-default-inline -D__GTHREAD_HIDE_WIN32API -mconsole -I/mingw/include -I/mingw/include/w32api -I/usr/local/include -mconsole -DNO_LINUX_RTC -DFS_CASEINSENSE'
CFLAGS='-mthreads -mms-bitfields -Os -mpreferred-stack-boundary=4 -mcpu=pentium3 -march=pentium-mmx -mmmx -fno-force-mem -fno-force-addr -fstrength-reduce -fexpensive-optimizations -fschedule-insns2 -fomit-frame-pointer -mconsole -I/mingw/include -I/mingw/include/w32api -I/usr/local/include -DNO_LINUX_RTC -DFS_CASEINSENSE'

echo Configuring for compile with: ${CXXFLAGS}
rm -f config.cache
CXXFLAGS=${CXXFLAGS} CFLAGS=${CFLAGS} LIBS='-L/usr/local/lib' ./configure --disable-shared --disable-dependency-tracking --disable-libtool-lock --disable-profile --enable-release $*
