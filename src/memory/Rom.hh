// $Id$

#ifndef __MSXROMDEVICE_HH__
#define __MSXROMDEVICE_HH__

#include <vector>
#include <cassert>
#include "RomInfo.hh"
#include "openmsx.hh"
#include "Debuggable.hh"

using std::vector;

namespace openmsx {

class EmuTime;
class Config;
class MSXRomPatchInterface;
class File;

class Rom : public Debuggable
{
public:
	Rom(const string& name, const string& description, Config* config);
	Rom(const string& name, const string& description, Config* config,
	    const string& filename);
	virtual ~Rom();

	const byte& operator[](unsigned address) const {
		assert(address < size);
		return rom[address];
	}

	const File* getFile() const {
		return file;
	}
	const RomInfo& getInfo() const {
		return *info;
	}

	const string& getName() const;

	// Debuggable
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

private:
	void read(Config* config, const string& filename);
	void init(const Config& config);
	
	string name;
	const string description;
	unsigned size;
	const byte* rom;

	File* file;
	vector<MSXRomPatchInterface*> romPatchInterfaces;
	auto_ptr<RomInfo> info;
};

} // namespace openmsx

#endif
