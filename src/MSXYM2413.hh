// $Id$

#ifndef __MSXYM2413_HH__
#define __MSXYM2413_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include "YM2413.hh"


class MSXYM2413 : public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXYM2413(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXYM2413(); 
		
		void reset(const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);

	protected:
		void writeRegisterPort(byte value, const EmuTime &time);
		void writeDataPort(byte value, const EmuTime &time);

		byte enable;
		YM2413 *ym2413;
		
	private:
		int registerLatch;
};
#endif
