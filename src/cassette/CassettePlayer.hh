// $Id$

#ifndef __CASSETTEPLAYER_HH__
#define __CASSETTEPLAYER_HH__

#include <SDL/SDL.h>
#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "CommandLineParser.hh"


class MSXCassettePlayerCLI : public CLIOption, public CLIFileType
{
	public:
		MSXCassettePlayerCLI();
		virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp() const;
		virtual void parseFileType(const std::string &filename);
		virtual const std::string& fileTypeHelp() const;
};


class CassettePlayer : public CassetteDevice, private Command
{
	public:
		CassettePlayer();
		virtual ~CassettePlayer();
		
		void insertTape(const std::string &filename);
		void removeTape();
		
		// CassetteDevice
		virtual void setMotor(bool status, const EmuTime &time);
		virtual short readSample(const EmuTime &time);
		virtual void writeWave(short *buf, int length);
		virtual int getWriteSampleRate();

		// Pluggable
		virtual const std::string &getName() const;

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
		virtual void help   (const std::vector<std::string> &tokens) const;
		virtual void tabCompletion(std::vector<std::string> &tokens) const;
};

#endif
