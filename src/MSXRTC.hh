// $Id$

#ifndef __MSXRTC_HH__
#define __MSXRTC_HH__

#include "MSXDevice.hh"
#include "EmuTime.hh"
#include "RP5C01.hh"


class MSXRTC : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXRTC(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXRTC(); 
		
		void init();
		void reset();
		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
	private:
		RP5C01 *rp5c01;
		nibble registerLatch;
};
#endif
