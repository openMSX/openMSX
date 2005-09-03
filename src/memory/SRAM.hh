// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Alarm.hh"
#include "Ram.hh"

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class CliComm;

class SRAM : private Alarm
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
	void write(unsigned addr, byte value) {
		if (!pending()) {
			schedule(5000000); // sync to disk after 5s
		}
		assert(addr < getSize());
		ram[addr] = value;
	}
	unsigned getSize() const {
		return ram.getSize();
	}

private:
	void load();
	void save();
	virtual void alarm();

	Ram ram;
	const XMLElement& config;
	const char* header;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
