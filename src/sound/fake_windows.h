#ifndef FAKE_WINDOWS_H
#define FAKE_WINDOWS_H

// Fake windows include file
//  meant to be used on non-windows systems to at least catch simple
//  compilation errors when changing windows specific files

typedef int HWND;
typedef unsigned short DWORD;
typedef int LPDIRECTSOUNDBUFFER;
typedef int LPDIRECTSOUND;

static const bool DS_OK = true;
static const bool DSSCL_EXCLUSIVE = 0;
static const bool DSERR_BUFFERLOST = true;
static const int DSCAPS_PRIMARY16BIT = 0;
static const int DSCAPS_SECONDARY16BIT = 0;
static const int DSCAPS_PRIMARYSTEREO = 0;
static const int DSCAPS_SECONDARYSTEREO = 0;
static const int WAVE_FORMAT_PCM = 0;
static const int DSBCAPS_PRIMARYBUFFER = 0;
static const int DSBCAPS_CTRLPOSITIONNOTIFY = 0;
static const int DSBCAPS_GETCURRENTPOSITION2 = 0;
static const int DSBCAPS_CTRLFREQUENCY = 0;
static const int DSBCAPS_CTRLPAN = 0;
static const int DSBCAPS_CTRLVOLUME = 0;
static const int DSBCAPS_GLOBALFOCUS = 0;
static const int DSBPLAY_LOOPING = 0;

struct DSCAPS {
	int dwSize; int dwFlags;
};
struct WF {
	int wFormatTag;
	int nChannels;
	int nSamplesPerSec;
	int nBlockAlign;
	int nAvgBytesPerSec;
};
struct PCMWAVEFORMAT {
	WF wf;
	int wBitsPerSample;
};
typedef PCMWAVEFORMAT* LPWAVEFORMATEX;
struct DSBUFFERDESC {
	int dwSize; int dwFlags; int dwBufferBytes;
	LPWAVEFORMATEX lpwfxFormat;
};
struct WAVEFORMATEX {
	int wFormatTag; int nChannels; int nSamplesPerSec;
	int wBitsPerSample; int nBlockAlign; int nAvgBytesPerSec;
};

bool DirectSoundCreate(int*, int*, int*);
bool IDirectSound_SetCooperativeLevel(int, int, int);
void IDirectSound_GetCaps(int, DSCAPS*);
bool IDirectSound_CreateSoundBuffer(int , DSBUFFERDESC*, int*, int*);
bool IDirectSoundBuffer_SetFormat(int, WAVEFORMATEX*);
void IDirectSoundBuffer_Stop(int);
void IDirectSoundBuffer_Release(int);
void IDirectSound_Release(int);
bool IDirectSoundBuffer_Lock(int, int, int, void*, DWORD*, void*, DWORD*, int);
void IDirectSoundBuffer_Restore(int);
void IDirectSoundBuffer_Unlock(int, void*, DWORD, void*, DWORD);
void IDirectSoundBuffer_GetCurrentPosition(int, DWORD*, DWORD*);
bool IDirectSoundBuffer_Play(int, int, int, int);

#endif
