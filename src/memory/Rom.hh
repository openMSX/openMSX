// $Id$

#ifndef __MSXROMDEVICE_HH__
#define __MSXROMDEVICE_HH__

#include <vector>
#include <cassert>
#include "RomInfo.hh"
#include "openmsx.hh"

using std::vector;

namespace openmsx {

class EmuTime;
class Config;
class MSXRomPatchInterface;
class File;

class Rom
{
public:
	Rom(Device* config);
	Rom(Device* config, const string& filename);
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
	const RomInfo& getInfo() const {
		return *info;
	}

private:
	void read(Device* config, const string& filename);
	
	const byte* rom;
	unsigned int size;
	File* file;
	vector<MSXRomPatchInterface*> romPatchInterfaces;
	RomInfo* info;
};

} // namespace openmsx

#endif
