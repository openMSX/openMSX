# $Id$

include build/node-start.mk

SUBDIRS:= \
	cassette commands config console cpu debugger events fdc file ide \
	input laserdisc memory resource security serial settings sound thread \
	utils video

SRC_HDR:= \
	EmuTime EmuDuration DynamicClock \
	MSXDevice \
	GlobalSettings \
	ThrottleManager \
	MSXMotherBoard \
	MSXPPI I8255 \
	DeviceFactory \
	Reactor Scheduler Schedulable \
	SaveStateCLI \
	MSXE6Timer \
	MSXResetStatusRegister \
	MSXTurboRPause \
	MSXS1985 \
	MSXS1990 \
	DummyDevice \
	MSXPrinterPort \
	PrinterPortDevice DummyPrinterPortDevice \
	PrinterPortSimpl PrinterPortLogger Printer \
	MSXKanji MSXKanji12 MSXBunsetsu \
	MSXRTC RP5C01 PasswordCart \
	RealTime \
	CartridgeSlotManager \
	MSXDeviceSwitch MSXSwitchedDevice \
	MSXMatsushita \
	MSXVictorHC9xSystemControl \
	MSXCielTurbo \
	FirmwareSwitch \
	CommandLineParser CLIOption \
	CliExtension \
	PluggingController Connector Pluggable PluggableFactory \
	DebugDevice \
	Autofire RenShaTurbo \
	EmptyPatch IPSPatch \
	Version \
	LedStatus \
	serialize serialize_meta serialize_core openmsx \
	ReverseManager ReplayCLI \
	MSXException

SRC_ONLY:= \
	main

HDR_ONLY:= \
	openmsx \
	Clock \
	InitException PlugException \
	PatchInterface I8255Interface \
	serialize_stl serialize_constr

DIST:= \
	Doxyfile

include build/node-end.mk
