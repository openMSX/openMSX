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
	MagicKey \
	Mouse \
	JoyNet \
	UserInputEventDistributor

HDR_ONLY:= \
	UserInputEventListener

include build/node-end.mk

