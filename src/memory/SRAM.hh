// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class CliComm;
class SRAMSync;

class SRAM
{
public:
	SRAM(MSXMotherBoard& motherBoard, const std::string& name, int size,
	     const XMLElement& config, const char* header = NULL);
	SRAM(MSXMotherBoard& motherBoard, const std::string& name,
	     const std::string& description, int size,
	     const XMLElement& config, const char* header = NULL);
	virtual ~SRAM();

	const byte& operator[](unsigned addr) const {
		assert(addr < getSize());
		return ram[addr];
	}
	// write() is non-inline because of the auto-sync to disk feature
	void write(unsigned addr, byte value);
	unsigned getSize() const {
		return ram.getSize();
	}

private:
	void load();
	void save();

	Ram ram;
	const XMLElement& config;
	const char* header;
	CliComm& cliComm;

	std::auto_ptr<SRAMSync> sramSync;
	friend class SRAMSync;
};

} // namespace openmsx

#endif
