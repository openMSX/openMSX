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
	JoyTap \
	JoyTapPort \
	NinjaTap \
	UserInputEventDistributor

HDR_ONLY:= \
	UserInputEventListener

include build/node-end.mk

