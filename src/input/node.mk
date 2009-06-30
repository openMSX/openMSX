# $Id$

include build/node-start.mk

SRC_HDR:= \
	Keyboard \
	KeyboardSettings \
	UnicodeKeymap \
	JoystickPort \
	JoystickDevice \
	DummyJoystick \
	Joystick \
	KeyJoystick \
	SETetrisDongle \
	MagicKey \
	Mouse \
	JoyTap \
	NinjaTap \
	ArkanoidPad \
	EventDelay \
	MSXEventDistributor \
	MSXEventRecorder \
	MSXEventReplayer \
	MSXEventRecorderReplayerCLI \
	RecordedCommand

HDR_ONLY:= \
	MSXEventListener

include build/node-end.mk

