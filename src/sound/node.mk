# $Id$

include build/node-start.mk

SRC_HDR:= \
	Mixer \
	SoundDevice \
	MSXPSG AY8910 \
	DACSound16S DACSound8U \
	KeyClick \
	SCC MSXSCCPlusCart \
	MSXAudio \
	Timer \
	Y8950 Y8950Adpcm Y8950KeyboardConnector Y8950KeyboardDevice \
	MC6850 \
	MSXFmPac MSXMusic \
	YM2413Core YM2413 YM2413_2 \
	MSXTurboRPCM \
	YMF262 YMF278 MSXMoonSound \
	AudioInputConnector AudioInputDevice DummyAudioInputDevice WavAudioInput

include build/node-end.mk

