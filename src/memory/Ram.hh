// $Id$

#ifndef RAM_HH
#define RAM_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class XMLElement;
class DeviceConfig;
class RamDebuggable;

class Ram : private noncopyable
{
public:
	/** Create Ram object with an associated debuggable. */
	Ram(const DeviceConfig& config, const std::string& name,
	    const std::string& description, unsigned size);

	/** Create Ram object without debuggable. */
	Ram(const DeviceConfig& config, unsigned size);

	~Ram();

	const byte& operator[](unsigned addr) const {
		return ram[addr];
	}
	byte& operator[](unsigned addr) {
		return ram[addr];
	}
	unsigned getSize() const {
		return unsigned(ram.size());
	}

	const std::string& getName() const;
	void clear(byte c = 0xff);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const XMLElement& xml;
	MemBuffer<byte> ram;
	const std::unique_ptr<RamDebuggable> debuggable;
};

} // namespace openmsx

#endif
