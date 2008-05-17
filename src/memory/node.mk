# $Id$

include build/node-start.mk

SRC_HDR:= \
	MSXMapperIO MSXMapperIOPhilips MSXMapperIOTurboR \
	MSXMemoryMapper \
	MSXRam \
	MSXRomCLI \
	Ram SRAM CheckedRam \
	Rom AmdFlash \
	RomInfo RomFactory RomDatabase \
	RomInfoTopic \
	MSXRom \
	Rom4kBBlocks Rom8kBBlocks Rom16kBBlocks \
	RomPlain RomPageNN RomGeneric8kB RomGeneric16kB \
	RomDRAM RomKonami4 RomKonami5 RomKonamiKeyboardMaster \
	RomAscii8kB RomAscii8_8 RomAscii16kB \
	RomPadial8kB RomPadial16kB RomMSXDOS2 \
	RomHydlide2 RomRType RomCrossBlaim RomHarryFox \
	RomGameMaster2 RomMajutsushi RomSynthesizer \
	RomKorean80in1 RomKorean90in1 RomKorean126in1 \
	RomPanasonic RomNational RomSuperLodeRunner \
	RomMSXAudio RomHalnote RomHolyQuran \
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

