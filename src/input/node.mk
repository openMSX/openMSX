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
	NinjaTap \
	EventTranslator \
	EventDelay \
	MSXEventDistributor

HDR_ONLY:= \
	MSXEventListener

include build/node-end.mk

