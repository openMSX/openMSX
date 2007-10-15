// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class MSXCliComm;
class SRAMSync;

class SRAM : private noncopyable
{
public:
	SRAM(MSXMotherBoard& motherBoard, const std::string& name, int size,
	     const XMLElement& config, const char* header = NULL,
	     bool* loaded = NULL);
	SRAM(MSXMotherBoard& motherBoard, const std::string& name,
	     const std::string& description, int size,
	     const XMLElement& config, const char* header = NULL,
	     bool* loaded = NULL);
	virtual ~SRAM();

	const byte& operator[](unsigned addr) const {
		assert(addr < getSize());
		return ram[addr];
	}
	// write() is non-inline because of the auto-sync to disk feature
	void write(unsigned addr, byte value);
	void memset(unsigned addr, byte c, unsigned size);
	unsigned getSize() const {
		return ram.getSize();
	}

private:
	void load(bool* loaded);
	void save();

	Ram ram;
	const XMLElement& config;
	const char* header;
	MSXCliComm& cliComm;

	std::auto_ptr<SRAMSync> sramSync;
	friend class SRAMSync;
};

} // namespace openmsx

#endif
