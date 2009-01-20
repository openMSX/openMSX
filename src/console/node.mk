# $Id$

include build/node-start.mk

SRC_HDR:= \
	Console CommandConsole \
	OSDConsoleRenderer \
	Font DummyFont \
	SDLConsole SDLFont \
	OSDGUILayer OSDGUI \
	OSDWidget OSDImageBasedWidget \
	OSDTopWidget OSDRectangle OSDText \
	TTFFont

SRC_HDR_$(COMPONENT_GL)+= \
	GLConsole GLFont

include build/node-end.mk

