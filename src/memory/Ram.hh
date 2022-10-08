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
	    static_string_view description, size_t size);

	/** Create Ram object without debuggable. */
	Ram(const XMLElement& xml, size_t size);

	[[nodiscard]] const byte& operator[](size_t addr) const {
		return ram[addr];
	}
	[[nodiscard]] byte& operator[](size_t addr) {
		return ram[addr];
	}
	[[nodiscard]] auto size()  const { return sz; }
	[[nodiscard]] auto data()        { return ram.data(); }
	[[nodiscard]] auto data()  const { return ram.data(); }
	[[nodiscard]] auto begin()       { return ram.data(); }
	[[nodiscard]] auto begin() const { return ram.data(); }
	[[nodiscard]] auto end()         { return ram.data() + sz; }
	[[nodiscard]] auto end()   const { return ram.data() + sz; }

	[[nodiscard]] const std::string& getName() const;
	void clear(byte c = 0xff);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const XMLElement& xml;
	MemBuffer<byte> ram;
	size_t sz; // must come before debuggable
	const std::optional<RamDebuggable> debuggable; // can be nullopt
};

} // namespace openmsx

#endif
