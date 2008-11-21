// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"
#include "EventListener.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class CliComm;
class SRAMSync;
class EventDistributor;

class SRAM : private EventListener, private noncopyable
{
public:
	SRAM(MSXMotherBoard& motherBoard, const std::string& name,
	     const std::string& description, int size);
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	void load(bool* loaded);
	void save();

	EventDistributor& eventDistributor;
	Ram ram;
	const XMLElement* config;
	const char* const header;
	CliComm& cliComm;

	const std::auto_ptr<SRAMSync> sramSync;
	friend class SRAMSync;
};

} // namespace openmsx

#endif
