// $Id$

#ifndef __MSXRTC_HH__
#define __MSXRTC_HH__

#include "MSXDevice.hh"
#include "emutime.hh"
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
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
	private:
		RP5C01 *rp5c01;
		nibble registerLatch;
};
#endif
