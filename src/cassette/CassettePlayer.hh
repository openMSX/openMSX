// $Id$

#ifndef __CASSETTEPLAYER_HH__
#define __CASSETTEPLAYER_HH__

#include <SDL/SDL.h>
#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include "Command.hh"


class CassettePlayer : public CassetteDevice, private Command
{
	public:
		CassettePlayer();
		virtual ~CassettePlayer();
		void insertTape(const std::string &filename);
		void removeTape();
		void setMotor(bool status, const EmuTime &time);
		short readSample(const EmuTime &time);
		void writeWave(short *buf, int length);
		int getWriteSampleRate();

		virtual const std::string &getName();

	private:
		int calcSamples(const EmuTime &time);
		
		// audio related variables
		SDL_AudioSpec audioSpec;
		Uint32 audioLength;	// 0 means no tape inserted
		Uint8 *audioBuffer;
		
		// player related variables
		bool motor;
		EmuTime timeReference;
		Uint32 posReference;

		// Tape Command
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens);
		virtual void tabCompletion(std::vector<std::string> &tokens);
};

#endif
