include build/node-start.mk

SRC_HDR:= \
	Keyboard \
	KeyboardSettings \
	UnicodeKeymap \
	JoystickPort \
	JoystickDevice \
	DummyJoystick \
	Joystick JoyMega \
	KeyJoystick \
	SETetrisDongle \
	MagicKey \
	Mouse \
	Trackball \
	Touchpad \
	JoyTap \
	NinjaTap \
	ArkanoidPad \
	EventDelay \
	MSXEventDistributor \
	StateChangeDistributor \
	RecordedCommand

HDR_ONLY:= \
	MSXEventListener \
	StateChangeListener \
	StateChange

include build/node-end.mk
