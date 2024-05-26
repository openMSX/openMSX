#ifndef SRAM_HH
#define SRAM_HH

#include "TrackedRam.hh"

#include "DeviceConfig.hh"
#include "RTSchedulable.hh"

#include <optional>

namespace openmsx {

class SRAM final
{
public:
	struct DontLoadTag {};
	SRAM(size_t size, const XMLElement& xml, DontLoadTag);
	SRAM(const std::string& name, static_string_view description,
	     size_t size, const DeviceConfig& config, DontLoadTag);
	SRAM(const std::string& name,
	     size_t size, const DeviceConfig& config, const char* header = nullptr,
	     bool* loaded = nullptr);
	SRAM(const std::string& name, static_string_view description,
	     size_t size, const DeviceConfig& config, const char* header = nullptr,
	     bool* loaded = nullptr);
	~SRAM();

	[[nodiscard]] const byte& operator[](size_t addr) const {
		assert(addr < size());
		return ram[addr];
	}
	// write() is non-inline because of the auto-sync to disk feature
	void write(size_t addr, byte value);
	void memset(size_t addr, byte c, size_t size);
	[[nodiscard]] size_t size() const {
		return ram.size();
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
	std::optional<SRAMSchedulable> schedulable;

	void load(bool* loaded);
	void save() const;

	const DeviceConfig config;
	TrackedRam ram;
	const char* const header = nullptr;
};

} // namespace openmsx

#endif
