#!/bin/sh

CXXFLAGS='-mthreads -mms-bitfields -Os -mpreferred-stack-boundary=4 -mcpu=pentium3 -march=pentium-mmx -mmmx -fno-force-mem -fno-force-addr -fstrength-reduce -fexpensive-optimizations -fschedule-insns2 -fomit-frame-pointer -fno-default-inline -D__GTHREAD_HIDE_WIN32API -mconsole -DNO_LINUX_RTC -DFS_CASEINSENSE'
CFLAGS='-mthreads -mms-bitfields -Os -mpreferred-stack-boundary=4 -mcpu=pentium3 -march=pentium-mmx -mmmx -fno-force-mem -fno-force-addr -fstrength-reduce -fexpensive-optimizations -fschedule-insns2 -fomit-frame-pointer -mconsole -DNO_LINUX_RTC -DFS_CASEINSENSE'

if test -d /mingw/include; then
	CXXFLAGS="${CXXFLAGS} -I/mingw/include"
	CFLAGS="${CFLAGS} -I/mingw/include"
	if test -d /mingw/include/w32api; then
		CXXFLAGS="${CXXFLAGS} -I/mingw/include/w32api"
		CFLAGS="${CFLAGS} -I/mingw/include/w32api"
	fi
fi
if test -d /usr/local/include; then
	CXXFLAGS="${CXXFLAGS} -I/usr/local/include"
	CFLAGS="${CFLAGS} -I/usr/local/include"
fi

if test -d /mingw/lib; then
	LIBS="${LIBS} -L/mingw/lib"
	if test -d /mingw/lib/w32api; then
		LIBS="${LIBS} -L/mingw/lib/w32api"
	fi
fi
if test -d /usr/local/lib; then
	LIBS="${LIBS} -L/usr/local/lib"
fi


echo Configuring for compile with: ${CXXFLAGS}
rm -f config.cache
CXXFLAGS="${CXXFLAGS}" CFLAGS="${CFLAGS}" LIBS="${LIBS}" ./configure --disable-shared --disable-dependency-tracking --disable-libtool-lock --disable-profile --enable-release $*
