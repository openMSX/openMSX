// $Id: CassettePlayer.cc,v

#include "CassettePlayer.hh"


CassettePlayer::CassettePlayer()
{
	motor = false;
	audioLength = 0;	// no tape inserted (yet)
}

CassettePlayer::CassettePlayer(const char* filename) 
{
	CassettePlayer();
	insertTape(filename);
}

CassettePlayer::~CassettePlayer()
{
	removeTape();	// free memory
}

void CassettePlayer::insertTape(const char* filename)
{
	// TODO throw exceptions instead of PRT_ERROR
	//      use String instead of char*
	if (audioLength != 0) 
		removeTape();
	if (SDL_LoadWAV(filename, &audioSpec, &audioBuffer, &audioLength) == NULL)
		PRT_ERROR("CassettePlayer error: " << SDL_GetError());
	if (audioSpec.format != AUDIO_S16)
		PRT_ERROR("CassettePlayer error: unsupported WAV format");
	posReference = 0;	// rewind tape (make configurable??)
}

void CassettePlayer::removeTape()
{
	SDL_FreeWAV(audioBuffer);
	audioLength = 0;
}

int CassettePlayer::calcSamples(const EmuTime &time)
{
	float duration = timeReference.getDuration(time);
	int samples = (int)(duration*audioSpec.freq);
	return posReference + samples;
}

void CassettePlayer::setMotor(bool status, const EmuTime &time)
{
	if (motor != status) {
		motor = status;
		if (motor) {
			// motor turned on
			timeReference = time;
		} else {
			// motor turned off
			posReference = calcSamples(time);
		}
	}
}

short CassettePlayer::readSample(const EmuTime &time)
{
	if (motor) {
		// motor on
		Uint32 index = calcSamples(time);
		if (index > (audioLength/2)) {
			return 0;
		} else {
			return ((short*)audioBuffer)[index];
		}
	} else {
		// motor off
		return 0;
	}
}

void CassettePlayer::writeWave(short *buf, int length)
{
	// recording not implemnted yet
}

int CassettePlayer::getWriteSampleRate()
{
	// recording not implemented yet
	return 0;	// 0 -> not interested in writeWave() data
}
