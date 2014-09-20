#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"
#include "DeviceConfig.hh"
#include "RTSchedulable.hh"

namespace openmsx {

class SRAM final : private RTSchedulable
{
public:
	enum DontLoad { DONT_LOAD };
	SRAM(const std::string& name, const std::string& description,
	     int size, const DeviceConfig& config, DontLoad);
	SRAM(const std::string& name,
	     int size, const DeviceConfig& config, const char* header = nullptr,
	     bool* loaded = nullptr);
	SRAM(const std::string& name, const std::string& description,
	     int size, const DeviceConfig& config, const char* header = nullptr,
	     bool* loaded = nullptr);
	~SRAM();

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
	// RTSchedulable
	void executeRT() override;

	void load(bool* loaded);
	void save();

	const DeviceConfig config;
	Ram ram;
	const char* const header;
};

} // namespace openmsx

#endif
