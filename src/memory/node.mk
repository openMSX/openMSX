# $Id$

include build/node-start.mk

SRC_HDR:= \
	MSXMapperIO MSXMapperIOPhilips MSXMapperIOTurboR \
	MSXMemoryMapper \
	MSXRam \
	MSXMirrorDevice \
	MSXRomCLI \
	Ram SRAM CheckedRam \
	Rom AmdFlash \
	RomInfo RomFactory RomDatabase \
	RomInfoTopic \
	MSXRom \
	RomBlocks \
	RomPlain RomPageNN RomGeneric8kB RomGeneric16kB \
	RomDRAM RomKonami RomKonamiSCC RomKonamiKeyboardMaster \
	RomAscii8kB RomAscii8_8 RomAscii16kB \
	RomPadial8kB RomPadial16kB RomMSXDOS2 \
	RomAscii16_2 RomRType RomCrossBlaim RomHarryFox \
	RomGameMaster2 RomMajutsushi RomSynthesizer \
	RomZemina80in1 RomZemina90in1 RomZemina126in1 \
	RomPanasonic RomNational RomSuperLodeRunner \
	RomHalnote RomHolyQuran \
	RomFSA1FM RomPlayBall RomNettouYakyuu \
	PanasonicMemory PanasonicRam \
	MSXMegaRam \
	MSXPac \
	MSXHBI55 \
	ESE_RAM ESE_SCC \
	RomManbow2 RomMatraInk

HDR_ONLY:= \
	RomTypes

include build/node-end.mk

