// $Id$

#ifndef __MSXAUDIO_HH__
#define __MSXAUDIO_HH__

#include "MSXIODevice.hh"
#include "CommandLineParser.hh"

// forward declaration
class Y8950;


class MSXAudioCLI : public CLIOption
{
	public:
		MSXAudioCLI();
		virtual void parseOption(const std::string &option,
			std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp();
};

class MSXAudio : public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXAudio(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXAudio(); 
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		Y8950 *y8950;
		int registerLatch;
};
#endif
