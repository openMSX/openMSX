// $Id$

#ifndef __MSXRTC_HH__
#define __MSXRTC_HH__

#include "MSXIODevice.hh"

// forward declaration
class RP5C01;
class EmuTime;


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
		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
	private:
		RP5C01 *rp5c01;
		nibble registerLatch;
};
#endif
