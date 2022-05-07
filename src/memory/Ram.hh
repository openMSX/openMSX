#ifndef RAM_HH
#define RAM_HH

#include "SimpleDebuggable.hh"
#include "MemBuffer.hh"
#include "openmsx.hh"
#include "static_string_view.hh"
#include <optional>
#include <string>

namespace openmsx {

class XMLElement;
class DeviceConfig;
class Ram;

class RamDebuggable final : public SimpleDebuggable
{
public:
	RamDebuggable(MSXMotherBoard& motherBoard, const std::string& name,
	              static_string_view description, Ram& ram);
	byte read(unsigned address) override;
	void write(unsigned address, byte value) override;
private:
	Ram& ram;
};

class Ram
{
public:
	/** Create Ram object with an associated debuggable. */
	Ram(const DeviceConfig& config, const std::string& name,
	    static_string_view description, unsigned size);

	/** Create Ram object without debuggable. */
	Ram(const XMLElement& xml, unsigned size);

	[[nodiscard]] const byte& operator[](unsigned addr) const {
		return ram[addr];
	}
	[[nodiscard]] byte& operator[](unsigned addr) {
		return ram[addr];
	}
	[[nodiscard]] unsigned getSize() const {
		return size;
	}

	[[nodiscard]] const std::string& getName() const;
	void clear(byte c = 0xff);

	void serialize(Archive auto& ar, unsigned version);

private:
	const XMLElement& xml;
	MemBuffer<byte> ram;
	unsigned size; // must come before debuggable
	const std::optional<RamDebuggable> debuggable; // can be nullopt
};

} // namespace openmsx

#endif
