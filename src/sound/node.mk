# $Id$

include build/node-start.mk

SRC_HDR:= \
	Mixer SDLSoundDriver NullSoundDriver \
	SoundDevice \
	MSXPSG AY8910 AY8910Periphery \
	DACSound16S DACSound8U \
	KeyClick \
	SCC MSXSCCPlusCart \
	MSXAudio \
	EmuTimer \
	Y8950 Y8950Adpcm Y8950KeyboardConnector Y8950KeyboardDevice \
	MSXFmPac MSXMusic \
	YM2413Core YM2413 YM2413_2 \
	MSXTurboRPCM \
	YMF262 YMF278 MSXMoonSound \
	AudioInputConnector AudioInputDevice DummyAudioInputDevice WavAudioInput

HDR_ONLY:= \
	SoundDriver

include build/node-end.mk

