# $Id$

include build/node-start.mk

SUBDIRS:= \
	cassette commands config console cpu debugger events fdc file ide \
	input memory serial settings sound thread video

SRC_HDR:= \
	EmuTime EmuDuration \
	MSXDevice \
	GlobalSettings \
	ThrottleManager \
	MSXMotherBoard \
	MSXPPI I8255 \
	PlatformFactory DeviceFactory \
	Reactor Scheduler Schedulable \
	MSXE6Timer \
	MSXF4Device \
	MSXTurboRPause \
	MSXS1985 \
	MSXS1990 \
	DummyDevice \
	MSXPrinterPort \
	PrinterPortDevice PrinterPortSimpl PrinterPortLogger \
	MSXKanji MSXKanji12 MSXBunsetsu \
	MSXRTC RP5C01 \
	RealTime \
	CartridgeSlotManager \
	MSXDeviceSwitch \
	MSXMatsushita \
	FirmwareSwitch \
	CommandLineParser \
	CliExtension \
	sha1 \
	CircularBuffer \
	PluggingController Connector Pluggable PluggableFactory \
	DebugDevice \
	Autofire RenShaTurbo \
	StringOp \
	Unicode \
	EmptyPatch IPSPatch \
	Version \
	HostCPU \
	PollInterface \

SRC_ONLY:= \
	main

HDR_ONLY:= \
	openmsx \
	likely \
	Clock DynamicClock \
	MSXException InitException \
	PatchInterface \

DIST:= \
	Doxyfile

include build/node-end.mk

