// $Id$

#ifndef __MSXPSG_HH__
#define __MSXPSG_HH__

#include "MSXDevice.hh"
#include "emutime.hh"
#include "AY8910.hh"
#include "JoystickPorts.hh"
#include "MSXCassettePort.hh"

class MSXPSG : public MSXDevice, AY8910Interface
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
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
	private:
		int registerLatch;
		AY8910 *ay8910;
	
	// AY8910Interface
	public:
		byte readA(const Emutime &time);
		byte readB(const Emutime &time);
		void writeA(byte value, const Emutime &time);
		void writeB(byte value, const Emutime &time);

	private:
		JoystickPorts *joyPorts;
		CassettePortInterface *cassette;
};
#endif
