// $Id$

#ifndef __MSXPSG_HH__
#define __MSXPSG_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include "AY8910.hh"
#include "JoystickPorts.hh"
#include "MSXCassettePort.hh"

class MSXPSG : public MSXIODevice, public AY8910Interface
{
	// MSXDevice
	public:
		/**
		 * Constructor
		 */
		MSXPSG(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXPSG(); 
		
		void init();
		void reset();
		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
	private:
		int registerLatch;
		AY8910 *ay8910;
	
	// AY8910Interface
	public:
		byte readA(const EmuTime &time);
		byte readB(const EmuTime &time);
		void writeA(byte value, const EmuTime &time);
		void writeB(byte value, const EmuTime &time);

	private:
		JoystickPorts *joyPorts;
		CassettePortInterface *cassette;
};
#endif
