// $Id$

#ifndef __MSXARCdebug_HH__
#define __MSXARCdebug_HH__

#include "MSXMemDevice.hh"
#include "FileOpener.hh"


class MSXARCdebug : public MSXMemDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXARCdebug(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXARCdebug();
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  

	private:
		IOFILETYPE* file;

};
#endif
