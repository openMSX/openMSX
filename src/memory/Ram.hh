#ifndef RAM_HH
#define RAM_HH

#include "SimpleDebuggable.hh"
#include "MemBuffer.hh"
#include "static_string_view.hh"

#include <cstdint>
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
	uint8_t read(unsigned address) override;
	void write(unsigned address, uint8_t value) override;
	void readBlock(unsigned start, std::span<uint8_t> output) override;
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

	[[nodiscard]] const uint8_t& operator[](size_t addr) const {
		return ram[addr];
	}
	[[nodiscard]] uint8_t& operator[](size_t addr) {
		return ram[addr];
	}
	[[nodiscard]] auto size()  const { return ram.size(); }
	[[nodiscard]] auto data()        { return ram.data(); }
	[[nodiscard]] auto data()  const { return ram.data(); }
	[[nodiscard]] auto begin()       { return ram.begin(); }
	[[nodiscard]] auto begin() const { return ram.begin(); }
	[[nodiscard]] auto end()         { return ram.end(); }
	[[nodiscard]] auto end()   const { return ram.end(); }

	[[nodiscard]] const std::string& getName() const;
	void clear(uint8_t c = 0xff);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const XMLElement& xml;
	MemBuffer<uint8_t> ram;
	const std::optional<RamDebuggable> debuggable; // can be nullopt
};

} // namespace openmsx

#endif
