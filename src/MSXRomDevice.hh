// $Id$

#ifndef __MSXROMDEVICE_HH__
#define __MSXROMDEVICE_HH__

#include <list>
#include "MSXConfig.hh"

// forward declaration
class MSXRomPatchInterface;
class File;


class MSXRomDevice
{
	public:
		MSXRomDevice(MSXConfig::Device *config, const EmuTime &time);
		MSXRomDevice(const std::string &filename, const EmuTime &time);
		virtual ~MSXRomDevice();

		byte read(unsigned int address) const {
			assert(address < size);
			return rom[address];
		}
		byte* getBlock(unsigned int address = 0) const {
			assert(address < size);
			return &rom[address];
		}
		int getSize() const {
			return size;
		}

	private:
		void read(MSXConfig::Device *config, 
		          const std::string &filename, const EmuTime &time);
		
		byte* rom;
		unsigned int size;
		File* file;
		std::list<MSXRomPatchInterface*> romPatchInterfaces;
};

#endif
