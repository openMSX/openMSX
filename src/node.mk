# $Id$

include build/node-start.mk

SUBDIRS:= \
	cassette commands config console cpu debugger events fdc file ide \
	input memory serial settings sound thread video

SRC_HDR:= \
	EmuTime \
	MSXDevice \
	GlobalSettings \
	MSXMotherBoard \
	MSXPPI I8255 \
	PlatformFactory DeviceFactory \
	Scheduler \
	MSXE6Timer \
	MSXF4Device \
	MSXTurboRLeds \
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
	IPS \
	Version \
	HostCPU

SRC_ONLY:= \
	main

HDR_ONLY:= \
	openmsx \
	Schedulable \
	MSXException InitException \

DIST:= \
	Doxyfile

include build/node-end.mk

