// $Id$

#ifndef ROM_HH
#define ROM_HH

#include "openmsx.hh"
#include "Debuggable.hh"
#include <memory>
#include <cassert>

namespace openmsx {

class MSXMotherBoard;
class EmuTime;
class XMLElement;
class File;
class RomInfo;
class CliComm;

class Rom : public Debuggable
{
public:
	Rom(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, const XMLElement& config);
	Rom(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, const XMLElement& config,
	    const std::string& id);
	virtual ~Rom();

	const byte& operator[](unsigned address) const {
		assert(address < size);
		return rom[address];
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
	void init(CliComm& cliComm, const XMLElement& config);
	void read(const XMLElement& config, const std::string& filename);
	bool checkSHA1(const XMLElement& config);

	MSXMotherBoard& motherBoard;
	std::string name;
	const std::string description;
	unsigned size;
	const byte* rom;
	byte* extendedRom;

	std::auto_ptr<File> file;
	std::auto_ptr<RomInfo> info;

	mutable std::string sha1sum;
};

} // namespace openmsx

#endif
