# $Id$

include build/node-start.mk

SRC_HDR:= \
	MSXMapperIO MSXMapperIOPhilips MSXMapperIOTurboR \
	MSXMemoryMapper \
	MSXRam \
	MSXRomCLI \
	SRAM \
	Rom \
	RomInfo RomFactory \
	MSXRom \
	Rom4kBBlocks Rom8kBBlocks Rom16kBBlocks \
	RomPlain RomPageNN RomGeneric8kB RomGeneric16kB \
	RomKonami4 RomKonami5 \
	RomAscii8kB RomAscii8_8 RomAscii16kB \
	RomHydlide2 RomRType RomCrossBlaim RomHarryFox \
	RomGameMaster2 RomMajutsushi RomSynthesizer \
	RomKorean80in1 RomKorean90in1 RomKorean126in1 \
	RomPanasonic RomNational \
	RomMSXAudio RomHalnote RomHolyQuran \
	RomFSA1FM \
	PanasonicMemory PanasonicRam PanasonicRom \
	MSXMegaRam \
	MSXPac \
	MSXHBI55

HDR_ONLY:= \
	RomTypes

include build/node-end.mk

