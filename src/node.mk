# $Id$

include build/node-start.mk

SUBDIRS:= \
	cassette commands config console cpu debugger events fdc file ide \
	input memory resource serial settings sound thread utils video

SRC_HDR:= \
	EmuTime EmuDuration \
	MSXDevice \
	GlobalSettings \
	ThrottleManager \
	MSXMotherBoard \
	MSXPPI I8255 \
	DeviceFactory \
	Reactor Scheduler Schedulable \
	MSXE6Timer \
	MSXF4Device \
	MSXTurboRPause \
	MSXS1985 \
	MSXS1990 \
	DummyDevice \
	MSXPrinterPort \
	PrinterPortDevice PrinterPortSimpl PrinterPortLogger \
	Printer \
	MSXKanji MSXKanji12 MSXBunsetsu \
	MSXRTC RP5C01 PasswordCart \
	RealTime \
	CartridgeSlotManager \
	MSXDeviceSwitch \
	MSXMatsushita \
	FirmwareSwitch \
	CommandLineParser CLIOption \
	CliExtension \
	PluggingController Connector Pluggable PluggableFactory \
	DebugDevice \
	Autofire RenShaTurbo \
	EmptyPatch IPSPatch \
	Version \
	LedStatus

SRC_ONLY:= \
	main

HDR_ONLY:= \
	openmsx \
	Clock DynamicClock \
	MSXException InitException PlugException \
	PatchInterface

DIST:= \
	Doxyfile

include build/node-end.mk
