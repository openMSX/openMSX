// $Id$

#ifndef __MSXROMDEVICE_HH__
#define __MSXROMDEVICE_HH__

#include <list>
#include "RomInfo.hh"
#include "openmsx.hh"

class EmuTime;
class Config;
class MSXRomPatchInterface;
class File;


class Rom
{
	public:
		Rom(Device *config, const EmuTime &time);
		Rom(Device *config, const string &filename,
		    const EmuTime &time);
		virtual ~Rom();

		byte read(unsigned int address) const {
			assert(address < size);
			return rom[address];
		}
		const byte* getBlock(unsigned int address = 0) const {
			assert(address < size);
			return &rom[address];
		}
		int getSize() const {
			return size;
		}
		const File* getFile() const {
			return file;
		}
		RomInfo getInfo() const {
			return *info;
		}

	private:
		void read(Device *config, 
		          const string &filename, const EmuTime &time);
		
		const byte* rom;
		unsigned int size;
		File* file;
		list<MSXRomPatchInterface*> romPatchInterfaces;
		RomInfo *info;
};

#endif
