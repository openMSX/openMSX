// $Id$

#ifndef __MSXROMDEVICE_HH__
#define __MSXROMDEVICE_HH__

#include <list>

class Config;
class MSXRomPatchInterface;
class File;


class MSXRomDevice
{
	public:
		MSXRomDevice(Device *config, const EmuTime &time);
		MSXRomDevice(Device *config, const std::string &filename,
		             const EmuTime &time);
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
		void read(Device *config, 
		          const std::string &filename, const EmuTime &time);
		
		byte* rom;
		unsigned int size;
		File* file;
		std::list<MSXRomPatchInterface*> romPatchInterfaces;
};

#endif
