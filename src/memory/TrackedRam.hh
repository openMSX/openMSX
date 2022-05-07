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
	           static_string_view description, unsigned size)
		: ram(config, name, description, size) {}

	TrackedRam(const XMLElement& xml, unsigned size)
		: ram(xml, size) {}

	[[nodiscard]] unsigned getSize() const {
		return ram.getSize();
	}

	[[nodiscard]] const std::string& getName() const {
		return ram.getName();
	}

	// Allow read via an explicit read() method or via backdoor access.
	[[nodiscard]] byte read(unsigned addr) const {
		return ram[addr];
	}

	[[nodiscard]] const byte& operator[](unsigned addr) const {
		return ram[addr];
	}

	// Only allow write/clear via an explicit method.
	void write(unsigned addr, byte value) {
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
	[[nodiscard]] byte* getWriteBackdoor() {
		writeSinceLastReverseSnapshot = true;
		return &ram[0];
	}

	void serialize(Archive auto& ar, unsigned version);

private:
	Ram ram;
	bool writeSinceLastReverseSnapshot = true;
};

} // namespace openmsx

#endif
