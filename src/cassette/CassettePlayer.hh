// $Id$

#ifndef __CASSETTEPLAYER_HH__
#define __CASSETTEPLAYER_HH__

#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include <SDL/SDL.h>

class CassettePlayer : public CassetteDevice
{
	public:
		CassettePlayer();
		CassettePlayer(const char* filename);
		virtual ~CassettePlayer();
		void insertTape(const char* filename);
		void removeTape();
		void setMotor(bool status, const EmuTime &time);
		short readSample(const EmuTime &time);
		void writeWave(short *buf, int length);
		int getWriteSampleRate();

		virtual const std::string &getName();

	private:
		int calcSamples(const EmuTime &time);
		
		// audio-related variables
		SDL_AudioSpec audioSpec;
		Uint32 audioLength;	// 0 means no tape inserted
		Uint8 *audioBuffer;
		
		// player-related variables
		bool motor;
		EmuTime timeReference;
		Uint32 posReference;
};

#endif
