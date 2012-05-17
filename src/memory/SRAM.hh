// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"
#include "DeviceConfig.hh"
#include "EventListener.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class CliComm;
class AlarmEvent;

class SRAM : private EventListener, private noncopyable
{
public:
	SRAM(MSXMotherBoard& motherBoard, const std::string& name,
	     const std::string& description, int size);
	SRAM(MSXMotherBoard& motherBoard, const std::string& name, int size,
	     const DeviceConfig& config, const char* header = NULL,
	     bool* loaded = NULL);
	SRAM(MSXMotherBoard& motherBoard, const std::string& name,
	     const std::string& description, int size,
	     const DeviceConfig& config, const char* header = NULL,
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
	virtual int signalEvent(const shared_ptr<const Event>& event);

	void load(bool* loaded);
	void save();

	const DeviceConfig config;
	Ram ram;
	const char* const header;
	CliComm& cliComm;

	const std::auto_ptr<AlarmEvent> sramSync;
};

} // namespace openmsx

#endif
