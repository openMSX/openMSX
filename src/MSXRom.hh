// $Id$
//
// Base class for MSX ROM Devices

#ifndef __MSXROM_HH__
#define __MSXROM_HH__

#include "MSXConfig.hh"
#include "MSXDevice.hh"
#include "LoadFile.hh"

// forward declaration
class MSXRomPatchInterface;

class MSXRom: virtual public MSXDevice, public LoadFile
{
	public:

		MSXRom();

		/**
		 * delete memory bank
		 */
		virtual ~MSXRom();

	protected:
		/**
		 * Return the MSXConfig object
		 */
		MSXConfig::Device* getDeviceConfig();

		/**
		 * Byte region used by inherited classes
		 * to store the ROM's memory bank.
		 */
		byte* memoryBank;

	private:
		/**
		 * create and register RomPatchInterfaces
		 * [called from constructor]
		 * for each patchcode parameter, construct apropriate patch
		 * object and register it at the CPU
		 */
		void handleRomPatchInterfaces();
		std::list<MSXRomPatchInterface*> romPatchInterfaces;
};

#endif // __MSXROM_HH__
