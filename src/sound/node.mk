# $Id$

include build/node-start.mk

SRC_HDR:= \
	Mixer MSXMixer NullSoundDriver \
	SDLSoundDriver DirectXSoundDriver \
	SoundDevice ResampledSoundDevice \
	ResampleTrivial ResampleHQ ResampleLQ ResampleBlip \
	BlipBuffer \
	MSXPSG AY8910 AY8910Periphery \
	DACSound16S DACSound8U \
	KeyClick \
	SCC MSXSCCPlusCart \
	VLM5030 \
	MSXAudio \
	EmuTimer \
	Y8950 Y8950Adpcm Y8950KeyboardConnector \
	Y8950Periphery \
	Y8950KeyboardDevice DummyY8950KeyboardDevice \
	MSXFmPac MSXMusic \
	YM2413 YM2413Okazaki YM2413Burczynski \
	MSXTurboRPCM \
	YMF262 YMF278 MSXMoonSound MSXOPL3Cartridge \
	YM2151 MSXYamahaSFG \
	AudioInputConnector AudioInputDevice \
	DummyAudioInputDevice WavAudioInput \
	WavWriter \
	SamplePlayer \
	WavData

HDR_ONLY:= \
	SoundDriver \
	ResampleAlgo \
	YM2413Core \

SRC_HDR_$(COMPONENT_AO)+= \
	LibAOSoundDriver

DIST:= \
	ResampleCoeffs.ii \
	ResampleHQ-x64.asm \
	ResampleHQ-x86.asm

#TODO
#TEST:= YM2413Test

include build/node-end.mk

