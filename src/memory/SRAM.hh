#ifndef SRAM_HH
#define SRAM_HH

#include "TrackedRam.hh"
#include "DeviceConfig.hh"
#include "RTSchedulable.hh"
#include <memory>

namespace openmsx {

class SRAM final
{
public:
	struct DontLoadTag {};
	SRAM(int size, const XMLElement& xml, DontLoadTag);
	SRAM(const std::string& name, static_string_view description,
	     int size, const DeviceConfig& config, DontLoadTag);
	SRAM(const std::string& name,
	     int size, const DeviceConfig& config, const char* header = nullptr,
	     bool* loaded = nullptr);
	SRAM(const std::string& name, static_string_view description,
	     int size, const DeviceConfig& config, const char* header = nullptr,
	     bool* loaded = nullptr);
	~SRAM();

	[[nodiscard]] const byte& operator[](unsigned addr) const {
		assert(addr < getSize());
		return ram[addr];
	}
	// write() is non-inline because of the auto-sync to disk feature
	void write(unsigned addr, byte value);
	void memset(unsigned addr, byte c, unsigned size);
	[[nodiscard]] unsigned getSize() const {
		return ram.getSize();
	}
	[[nodiscard]] const std::string& getLoadedFilename() const {
		return loadedFilename;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct SRAMSchedulable final : public RTSchedulable {
		explicit SRAMSchedulable(RTScheduler& scheduler_, SRAM& sram_)
			: RTSchedulable(scheduler_), sram(sram_) {}
		void executeRT() override;
	private:
		SRAM& sram;
	};
	std::unique_ptr<SRAMSchedulable> schedulable;

	void load(bool* loaded);
	void save();

	const DeviceConfig config;
	TrackedRam ram;
	const char* const header;

	std::string loadedFilename;
};

} // namespace openmsx

#endif
