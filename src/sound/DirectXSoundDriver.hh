#ifndef DIRECTXSOUNDDRIVER_HH
#define DIRECTXSOUNDDRIVER_HH
#ifdef _WIN32

#include "SoundDriver.hh"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN // Needed for <dsound.h>
#endif
#define DIRECTSOUND_VERSION 0x0500
#include <windows.h>
#include <dsound.h>

namespace openmsx {

class DirectXSoundDriver final : public SoundDriver
{
public:
	DirectXSoundDriver(const DirectXSoundDriver&) = delete;
	DirectXSoundDriver& operator=(const DirectXSoundDriver&) = delete;

	DirectXSoundDriver(unsigned sampleRate, unsigned bufferSize);
	~DirectXSoundDriver();

	void mute() override;
	void unmute() override;

	unsigned getFrequency() const override;
	unsigned getSamples() const override;

	void uploadBuffer(int16_t* buffer, unsigned len) override;

private:
	void dxClear();
	int dxCanWrite(unsigned start, unsigned size);
	void dxWriteOne(int16_t* buffer, unsigned lockSize);

	enum DxState { DX_SOUND_DISABLED, DX_SOUND_ENABLED, DX_SOUND_RUNNING };
	DxState state;
	unsigned bufferOffset;
	unsigned bufferSize;
	unsigned fragmentSize;
	int skipCount;
	LPDIRECTSOUNDBUFFER primaryBuffer;
	LPDIRECTSOUNDBUFFER secondaryBuffer;
	LPDIRECTSOUND directSound;

	unsigned frequency;
};

} // namespace openmsx

#endif // _WIN32
#endif // DIRECTXSOUNDDRIVER_HH
