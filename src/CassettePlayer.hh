// $Id: CassettePlayer.hh,v

#ifndef __CASSETTEPLAYER_HH__
#define __CASSETTEPLAYER_HH__

#include "CassetteDevice.hh"
#include "SDL/SDL.h"

class CassettePlayer : public CassetteDevice
{
	public:
		CassettePlayer();
		CassettePlayer(const char* filename);
		virtual ~CassettePlayer();
		void insertTape(const char* filename);
		void removeTape();
		void setMotor(bool status, const Emutime &time);
		short readSample(const Emutime &time);
		void writeWave(short *buf, int length);
		int getWriteSampleRate();

	private:
		int calcSamples(const Emutime &time);
		
		// audio-related variables
		SDL_AudioSpec audioSpec;
		Uint32 audioLength;	// 0 means no tape inserted
		Uint8 *audioBuffer;
		
		// player-related variables
		bool motor;
		Emutime timeReference;
		Uint32 posReference;
};

#endif
