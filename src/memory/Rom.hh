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
class XMLElement;
class MSXRomPatchInterface;
class File;

class Rom : public Debuggable
{
public:
	Rom(const string& name, const string& description, const XMLElement& config);
	Rom(const string& name, const string& description, const XMLElement& config,
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
	const string& getSHA1Sum() const;
	
	// Debuggable
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

private:
	void read(const XMLElement& config, const string& filename);
	void init(const XMLElement& config);
	bool checkSHA1(const XMLElement& config);
	void patch(const XMLElement& config);
	
	string name;
	const string description;
	unsigned size;
	const byte* rom;

	File* file;
	vector<MSXRomPatchInterface*> romPatchInterfaces;
	auto_ptr<RomInfo> info;

	mutable string sha1sum;
};

} // namespace openmsx

#endif
