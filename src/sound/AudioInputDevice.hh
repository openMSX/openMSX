#ifndef AUDIOINPUTDEVICE_HH
#define AUDIOINPUTDEVICE_HH

#include "Pluggable.hh"
#include <cstdint>

namespace openmsx {

class AudioInputDevice : public Pluggable
{
public:
	/**
	 * Read wave data
	 */
	[[nodiscard]] virtual int16_t readSample(EmuTime time) = 0;

	// Pluggable
	[[nodiscard]] std::string_view getClass() const final;
};

} // namespace openmsx

#endif
