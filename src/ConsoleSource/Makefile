#Makefile for the SDL_Console library

INCLUDES = `sdl-config --cflags`
CC = g++
OBJS = CON_console.o CON_consolecommands.o DT_drawtext.o CON_internal.o
CFLAGS = -g
CPPFLAGS = $(INCLUDES)

all: libSDL_console.a

libSDL_console.a: $(OBJS)
	ar cru libSDL_console.a $(OBJS)

clean:
	$(RM) core $(OBJS) libSDL_console.a
