# $Id$

include build/node-start.mk

SRC_HDR:= \
	Keyboard \
	JoystickPort \
	JoystickDevice \
	DummyJoystick \
	Joystick \
	KeyJoystick \
	SETetrisDongle \
	Mouse \
	JoyNet \
	UserInputEventDistributor

HDR_ONLY:= \
	UserInputEventListener

include build/node-end.mk

