// $Id$

#ifndef __MSXROMDEVICE_HH__
#define __MSXROMDEVICE_HH__

#include <memory>
#include <cassert>
#include "openmsx.hh"
#include "Debuggable.hh"

namespace openmsx {

class EmuTime;
class XMLElement;
class File;
class RomInfo;

class Rom : public Debuggable
{
public:
	Rom(const std::string& name, const std::string& description,
	    const XMLElement& config);
	Rom(const std::string& name, const std::string& description,
	    const XMLElement& config, const std::string& id);
	virtual ~Rom();

	const byte& operator[](unsigned address) const {
		assert(address < size);
		return rom[address];
	}

	const File* getFile() const {
		return file.get();
	}
	const RomInfo& getInfo() const {
		return *info;
	}

	const std::string& getName() const;
	const std::string& getSHA1Sum() const;
	
	// Debuggable
	virtual unsigned getSize() const;
	virtual const std::string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

private:
	void init(const XMLElement& config);
	void read(const XMLElement& config, const std::string& filename);
	void readIPS(const XMLElement& config, const std::string& filename);
	bool checkSHA1(const XMLElement& config);
	
	std::string name;
	const std::string description;
	unsigned size;
	const byte* rom;

	std::auto_ptr<File> file;
	std::auto_ptr<RomInfo> info;

	mutable std::string sha1sum;
};

} // namespace openmsx

#endif
