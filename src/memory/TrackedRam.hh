#ifndef TRACKED_RAM_HH
#define TRACKED_RAM_HH

#include "Ram.hh"

namespace openmsx {

// Ram with dirty tracking
class TrackedRam
{
public:
	// Most methods simply delegate to the internal 'ram' object.
	TrackedRam(const DeviceConfig& config, const std::string& name,
	           static_string_view description, size_t size)
		: ram(config, name, description, size) {}

	TrackedRam(const XMLElement& xml, size_t size)
		: ram(xml, size) {}

	[[nodiscard]] size_t size() const {
		return ram.size();
	}

	[[nodiscard]] const std::string& getName() const {
		return ram.getName();
	}

	// Allow read via an explicit read() method or via backdoor access.
	[[nodiscard]] byte read(size_t addr) const {
		return ram[addr];
	}

	[[nodiscard]] const byte& operator[](size_t addr) const {
		return ram[addr];
	}

	[[nodiscard]] auto begin() const { return ram.begin(); }
	[[nodiscard]] auto end()   const { return ram.end(); }

	// Only allow write/clear via an explicit method.
	void write(size_t addr, byte value) {
		writeSinceLastReverseSnapshot = true;
		ram[addr] = value;
	}

	void clear(byte c = 0xff) {
		writeSinceLastReverseSnapshot = true;
		ram.clear(c);
	}

	// Some write operations are more efficient in bulk. For those this
	// method can be used. It will mark the ram as dirty on each
	// invocation, so the resulting pointer (although the same each time)
	// should not be reused for multiple (distinct) bulk write operations.
	[[nodiscard]] std::span<byte> getWriteBackdoor() {
		writeSinceLastReverseSnapshot = true;
		return {ram.data(), size()};
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Ram ram;
	bool writeSinceLastReverseSnapshot = true;
};

} // namespace openmsx

#endif
