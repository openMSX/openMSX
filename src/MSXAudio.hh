// $Id$

#ifndef __MSXAUDIO_HH__
#define __MSXAUDIO_HH__

#include "MSXIODevice.hh"

// forward declaration
class Y8950;

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
		~MSXAudio(); 
		
		void reset(const EmuTime &time);
		
		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		Y8950 *y8950;
		int registerLatch;
};
#endif
