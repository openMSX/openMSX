#ifndef DIRECTXSOUNDDRIVER_HH
#define DIRECTXSOUNDDRIVER_HH
#ifdef _WIN32

#include "SoundDriver.hh"
#include "noncopyable.hh"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN // Needed for <dsound.h>
#endif
#define DIRECTSOUND_VERSION 0x0500
#include <windows.h>
#include <dsound.h>

namespace openmsx {

class DirectXSoundDriver final : public SoundDriver, private noncopyable
{
public:
	DirectXSoundDriver(unsigned sampleRate, unsigned bufferSize);
	virtual ~DirectXSoundDriver();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual void uploadBuffer(short* buffer, unsigned len);

private:
	void dxClear();
	int dxCanWrite(unsigned start, unsigned size);
	void dxWriteOne(short* buffer, unsigned lockSize);

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
