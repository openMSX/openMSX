// $Id$

#ifndef __MSXYM2413_HH__
#define __MSXYM2413_HH__

#ifndef VERSION
#include "config.h"
#endif

#if !defined(DONT_WANT_FMPAC) || !defined(DONT_WANT_MSXMUSIC)

#include "MSXIODevice.hh"

// forward declaration
class YM2413;


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
		virtual ~MSXYM2413(); 
		
		virtual void reset(const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	protected:
		void writeRegisterPort(byte value, const EmuTime &time);
		void writeDataPort(byte value, const EmuTime &time);

		byte enable;
		YM2413 *ym2413;
		
	private:
		int registerLatch;
};

#endif // not defined(DONT_WANT_FMPAC) || not defined(DONT_WANT_MSXMUSIC)

#endif
