# $Id$

include build/node-start.mk

SRC_HDR:= \
	Console CommandConsole \
	OSDConsoleRenderer \
	OSDGUILayer OSDGUI \
	OSDWidget OSDImageBasedWidget \
	OSDTopWidget OSDRectangle OSDText \
	TTFFont

include build/node-end.mk

