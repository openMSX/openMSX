# $Id$

include build/node-start.mk

SRC_HDR:= \
	Console CommandConsole \
	ConsoleRenderer OSDConsoleRenderer \
	Font DummyFont \
	SDLConsole SDLFont

SRC_HDR_$(COMPONENT_GL)+= \
	GLConsole GLFont

include build/node-end.mk

