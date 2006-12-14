// $Id$

#ifdef _WIN32

#include "DirectXSoundDriver.hh"
#include "MSXException.hh"
#include "openmsx.hh"
#include <string.h>
#include <SDL.h>       //
#include <SDL_syswm.h> //

namespace openmsx {

static const int BYTES_PER_SAMPLE = 2;
static const int CHANNELS = 2;

static HWND getWindowHandle()
{
	// This is SDL specific code, refactor when needed

	// !! Initialize video system, DirectX needs a handle to the window
	// !! and this only works when SDL video part is initialized
	if (!SDL_WasInit(SDL_INIT_VIDEO)) SDL_InitSubSystem(SDL_INIT_VIDEO);

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!SDL_GetWMInfo(&info)) {
		throw MSXException("Couldn't initialize DirectSound driver");
	}
	return info.window;
}


DirectXSoundDriver::DirectXSoundDriver(unsigned sampleRate, unsigned samples)
{
	if (DirectSoundCreate(NULL, &directSound, NULL) != DS_OK) {
		throw MSXException("Couldn't initialize DirectSound driver");
	}
	HWND hwnd = getWindowHandle();
	if (IDirectSound_SetCooperativeLevel(
				directSound, hwnd, DSSCL_EXCLUSIVE) != DS_OK) {
		throw MSXException("Couldn't initialize DirectSound driver");
	}

	DSCAPS capabilities;
	memset(&capabilities, 0, sizeof(DSCAPS));
	capabilities.dwSize = sizeof(DSCAPS);
	IDirectSound_GetCaps(directSound, &capabilities);
	if (!((capabilities.dwFlags & DSCAPS_PRIMARY16BIT) ||
	      (capabilities.dwFlags & DSCAPS_SECONDARY16BIT))) {
		// no 16 bits per sample
		throw MSXException("Couldn't configure 16 bit per sample");
	}
	if (!((capabilities.dwFlags & DSCAPS_PRIMARYSTEREO) ||
	      (capabilities.dwFlags & DSCAPS_SECONDARYSTEREO))) {
		// no stereo
		throw MSXException("Couldn't configure stereo mode");
	}

	PCMWAVEFORMAT pcmwf;
	memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = CHANNELS;
	pcmwf.wf.nSamplesPerSec = sampleRate;
	pcmwf.wBitsPerSample = 8 * BYTES_PER_SAMPLE;
	pcmwf.wf.nBlockAlign = CHANNELS * BYTES_PER_SAMPLE;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;

	DSBUFFERDESC desc;
	memset(&desc, 0, sizeof(DSBUFFERDESC));
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	bufferSize = 2 * samples * BYTES_PER_SAMPLE * CHANNELS;
	fragmentSize = 1;
	while (bufferSize / fragmentSize >= 32 || fragmentSize < 512) {
		fragmentSize <<= 1;
	}
	DWORD fragmentCount = 1 + bufferSize / fragmentSize;
	while (fragmentCount < 8) {
		fragmentCount *= 2;
		fragmentSize /= 2;
	}
	bufferSize = fragmentCount * fragmentSize;

	if (IDirectSound_CreateSoundBuffer(
			directSound, &desc, &secondaryBuffer, NULL) != DS_OK) {
		throw MSXException("Couldn't initialize DirectSound driver");
	}

	memset(&desc, 0, sizeof(DSBUFFERDESC));
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2
	             | DSBCAPS_CTRLFREQUENCY      | DSBCAPS_CTRLPAN
	             | DSBCAPS_CTRLVOLUME         | DSBCAPS_GLOBALFOCUS;
	desc.dwBufferBytes = bufferSize;
	desc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;

	if (IDirectSound_CreateSoundBuffer(
			directSound, &desc, &primaryBuffer, NULL) != DS_OK) {
		throw MSXException("Couldn't initialize DirectSound driver");
	}

	WAVEFORMATEX wfex;
	memset(&wfex, 0, sizeof(WAVEFORMATEX));
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = CHANNELS;
	wfex.nSamplesPerSec = sampleRate;
	wfex.wBitsPerSample = 8 * BYTES_PER_SAMPLE;
	wfex.nBlockAlign = CHANNELS * BYTES_PER_SAMPLE;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

	if (IDirectSoundBuffer_SetFormat(secondaryBuffer, &wfex) != DS_OK) {
		throw MSXException("Couldn't initialize DirectSound driver");
	}

	bufferOffset = bufferSize;
	dxClear();
	skipCount = 0;
	state = DX_SOUND_DISABLED;

	frequency = sampleRate;
	mixBuffer = new short[bufferSize / BYTES_PER_SAMPLE];
}

DirectXSoundDriver::~DirectXSoundDriver()
{
	delete[] mixBuffer;

	IDirectSoundBuffer_Stop(primaryBuffer);
	IDirectSoundBuffer_Release(primaryBuffer);
	IDirectSound_Release(directSound);
}

void DirectXSoundDriver::mute()
{
	dxClear();
	IDirectSoundBuffer_Stop(primaryBuffer);
	state = DX_SOUND_DISABLED;
}

void DirectXSoundDriver::unmute()
{
	state = DX_SOUND_ENABLED;
}

unsigned DirectXSoundDriver::getFrequency() const
{
	return frequency;
}

unsigned DirectXSoundDriver::getSamples() const
{
	return fragmentSize;
}

void DirectXSoundDriver::dxClear()
{
	void *audioBuffer1, *audioBuffer2;
	DWORD audioSize1, audioSize2;
	if (IDirectSoundBuffer_Lock(
			primaryBuffer, 0, bufferSize,
			&audioBuffer1, &audioSize1,
			&audioBuffer2, &audioSize2, 0) == DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(primaryBuffer);
	} else {
		memset(audioBuffer1, 0, audioSize1);
		if (audioBuffer2) {
			memset(audioBuffer2, 0, audioSize2);
		}
		IDirectSoundBuffer_Unlock(
				primaryBuffer, audioBuffer1, audioSize1,
				audioBuffer2, audioSize2);
	}
}

int DirectXSoundDriver::dxCanWrite(unsigned start, unsigned size)
{
	DWORD readPos, writePos;
	IDirectSoundBuffer_GetCurrentPosition(
			primaryBuffer, &readPos, &writePos);
	unsigned end = start + size;
	if (writePos < readPos) writePos += bufferSize;
	if (start    < readPos) start    += bufferSize;
	if (end      < readPos) end      += bufferSize;

	if ((start < writePos) || (end < writePos)) {
		return (bufferSize - (writePos - readPos)) / 2 - fragmentSize;
	} else {
		return 0;
	}
}

void DirectXSoundDriver::dxWriteOne(short* buffer, unsigned lockSize)
{
	void *audioBuffer1, *audioBuffer2;
	DWORD audioSize1, audioSize2;
	do {
		if (IDirectSoundBuffer_Lock(
				primaryBuffer, bufferOffset, lockSize,
				&audioBuffer1, &audioSize1, &audioBuffer2,
				&audioSize2, 0) == DSERR_BUFFERLOST) {
			IDirectSoundBuffer_Restore(primaryBuffer);
			IDirectSoundBuffer_Lock(
				primaryBuffer, bufferOffset, lockSize,
				&audioBuffer1, &audioSize1, &audioBuffer2,
				&audioSize2, 0);
		}
	} while ((audioSize1 + audioSize2) < lockSize);

	memcpy(audioBuffer1, buffer, audioSize1);
	if (audioBuffer2) {
		memcpy(audioBuffer2, (byte*)buffer + audioSize1, audioSize2);
	}
	IDirectSoundBuffer_Unlock(primaryBuffer, audioBuffer1, audioSize1,
			audioBuffer2, audioSize2);
	bufferOffset += lockSize;
	bufferOffset %= bufferSize;
}

double DirectXSoundDriver::uploadBuffer(short* buffer, unsigned count)
{
	if (state == DX_SOUND_DISABLED) return 1.0;

	if (state == DX_SOUND_ENABLED) {
		DWORD readPos, writePos;
		IDirectSoundBuffer_GetCurrentPosition(
				primaryBuffer, &readPos, &writePos);
		bufferOffset = (readPos + bufferSize / 2) % bufferSize;

		if (IDirectSoundBuffer_Play(primaryBuffer, 0, 0,
					DSBPLAY_LOOPING) == DSERR_BUFFERLOST) {
			IDirectSoundBuffer_Play(
					primaryBuffer, 0, 0, DSBPLAY_LOOPING);
		}
		state = DX_SOUND_RUNNING;
	}

	count *= (CHANNELS * BYTES_PER_SAMPLE);
	if (skipCount > 0) {
		skipCount -= count;
		return 1.0;
	}
	skipCount = dxCanWrite(bufferOffset, count);
	if (skipCount <= 0) {
		dxWriteOne(buffer, count);
	}
	return 1.0;
}

} // namespace openmsx

#endif // _WIN32
