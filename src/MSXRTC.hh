// $Id$

#ifndef __MSXRTC_HH__
#define __MSXRTC_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include "RP5C01.hh"


class MSXRTC : public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXRTC(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXRTC(); 

		void reset(const EmuTime &time);
		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
	private:
		RP5C01 *rp5c01;
		nibble registerLatch;
};
#endif
