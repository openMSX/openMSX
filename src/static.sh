#!/bin/sh

# make a completely static binary

c++ -Wall -DDEBUG -g -fno-rtti -pipe -g -O2 -o s_openmsx \
 `ls *.o` \
 -Xlinker -static \
 -L/usr/lib -lxml++ -lxml2 -lz -L/lib -lSDL -lm -L/usr/X11R6/lib -lX11 -lXext -lXxf86vm -lXxf86dga -lXv -ldl -lpthread
