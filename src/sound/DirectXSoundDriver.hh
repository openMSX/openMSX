// $Id$

#ifndef DIRECTXSOUNDDRIVER_HH
#define DIRECTXSOUNDDRIVER_HH
#ifdef _WIN32

#include "SoundDriver.hh"
#include "Schedulable.hh"
#define DIRECTSOUND_VERSION 0x0500
#include <windows.h>
#include <dsound.h>

/*
// dummy types, functions
// to easy development on non-windows systems
// (you can at least check the code on compilation errors)
typedef int LPDIRECTSOUNDBUFFER;
typedef int LPDIRECTSOUND;
typedef int HWND;
typedef int LPWAVEFORMATEX;
static const int DSERR_BUFFERLOST = -1;
static const int DSBPLAY_LOOPING = 1;
static const int DSSCL_EXCLUSIVE = 2;
static const int DSCAPS_PRIMARY16BIT = 1;
static const int DSCAPS_SECONDARY16BIT = 2;
static const int DSCAPS_PRIMARYSTEREO = 4;
static const int DSCAPS_SECONDARYSTEREO = 8;
static const int DSBCAPS_PRIMARYBUFFER = 1;
static const int DSBCAPS_CTRLPOSITIONNOTIFY = 1;
static const int DSBCAPS_GETCURRENTPOSITION2 = 2;
static const int DSBCAPS_CTRLFREQUENCY = 4;
static const int DSBCAPS_CTRLPAN = 8;
static const int DSBCAPS_CTRLVOLUME = 16;
static const int DSBCAPS_GLOBALFOCUS = 32;
static const int WAVE_FORMAT_PCM = 1;
static const int DS_OK = 1;
struct DSCAPS { int dwSize; int dwFlags; };
struct WF { int wFormatTag; int nChannels; int nSamplesPerSec; int nBlockAlign;
            int nAvgBytesPerSec; };
struct PCMWAVEFORMAT { int wBitsPerSample; WF wf; };
struct DSBUFFERDESC { int dwSize; int dwFlags; int dwBufferBytes; int lpwfxFormat; };
struct WAVEFORMATEX { int wFormatTag; int nChannels; int nSamplesPerSec; int wBitsPerSample;
	int nBlockAlign; int nAvgBytesPerSec; };

int IDirectSoundBuffer_Lock(LPDIRECTSOUNDBUFFER, int, int, void*, unsigned*, void*, unsigned*, int);
int IDirectSoundBuffer_Restore(LPDIRECTSOUNDBUFFER);
int IDirectSoundBuffer_Unlock(LPDIRECTSOUNDBUFFER, void*, unsigned, void*, unsigned);
int IDirectSoundBuffer_GetCurrentPosition(LPDIRECTSOUNDBUFFER, unsigned*, unsigned*);
int IDirectSoundBuffer_Play(LPDIRECTSOUNDBUFFER, int, int, int);
int DirectSoundCreate(void*, LPDIRECTSOUND*, void*);
int IDirectSound_SetCooperativeLevel(LPDIRECTSOUND, HWND, int);
int IDirectSound_GetCaps(LPDIRECTSOUND, DSCAPS*);
int IDirectSound_CreateSoundBuffer(LPDIRECTSOUND, DSBUFFERDESC*, LPDIRECTSOUNDBUFFER*, void*);
int IDirectSoundBuffer_SetFormat(LPDIRECTSOUNDBUFFER, WAVEFORMATEX*);
int IDirectSoundBuffer_Stop(LPDIRECTSOUNDBUFFER);
int IDirectSoundBuffer_Release(LPDIRECTSOUNDBUFFER);
int IDirectSound_Release(LPDIRECTSOUND);
*/


namespace openmsx {

class Mixer;
class IntegerSetting;
class BooleanSetting;

class DirectXSoundDriver : public SoundDriver, private Schedulable
{
public:
	DirectXSoundDriver(Mixer& mixer, unsigned sampleRate, unsigned bufferSize);
	virtual ~DirectXSoundDriver();

	virtual void lock();
	virtual void unlock();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual void updateStream(const EmuTime& time);

private:
	void dxClear();
	int dxCanWrite(unsigned start, unsigned size);
	void dxWriteOne(short* buffer, unsigned lockSize);
	void dxWrite(short* buffer, unsigned count);
	void reInit();

	// Schedulable
	void executeUntil(const EmuTime& time, int userData);
	const std::string& schedName() const;

	enum DxState { DX_SOUND_DISABLED, DX_SOUND_ENABLED, DX_SOUND_RUNNING };
	DxState state;
	unsigned bufferOffset;
	unsigned bufferSize;
	unsigned fragmentCount;
	unsigned fragmentSize;
	int  skipCount;
	LPDIRECTSOUNDBUFFER primaryBuffer;
	LPDIRECTSOUNDBUFFER secondaryBuffer;
	LPDIRECTSOUND directSound;
	
	Mixer& mixer;
	unsigned frequency;

	short* mixBuffer;
	EmuTime prevTime;
	EmuDuration interval1;
	//EmuDuration intervalAverage;
	
	IntegerSetting& speedSetting;
	//BooleanSetting& throttleSetting;
};

} // namespace openmsx

#endif // _WIN32
#endif // DIRECTXSOUNDDRIVER_HH
