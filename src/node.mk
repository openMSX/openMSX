# $Id$

SUBDIRS:= \
	cassette config console cpu debugger events fdc file ide input \
	libxmlx memory serial settings sound thread video

SRC_HDR:= \
	Icon \
	EmuTime \
	MSXDevice MSXIODevice MSXMemDevice \
	MSXMotherBoard \
	MSXPPI I8255 \
	PlatformFactory DeviceFactory \
	Scheduler Schedulable \
	Leds \
	MSXE6Timer \
	MSXF4Device \
	MSXTurboRLeds \
	MSXTurboRPause \
	MSXS1985 \
	MSXS1990 \
	DummyDevice \
	JoystickPorts \
	MSXPrinterPort \
	PrinterPortDevice PrinterPortSimpl PrinterPortLogger \
	MSXKanji MSXKanji12 MSXBunsetsu \
	MSXRTC RP5C01 \
	RealTime RealTimeSDL RealTimeRTC \
	CartridgeSlotManager \
	MSXDeviceSwitch \
	MSXMatsushita \
	FrontSwitch \
	CommandLineParser \
	CliExtension \
	MSXDiskRomPatch \
	md5 sha1 \
	CircularBuffer \
	PluggingController Connector Pluggable \
	DebugDevice

SRC_ONLY:= \
	main.cc

HDR_ONLY:= \
	openmsx.hh \
	util.hh \
	MSXException.hh \
	MSXRomPatchInterface.hh \
	config.h

$(eval $(PROCESS_NODE))

