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
	virtual int16_t readSample(EmuTime::param time) = 0;

	// Pluggable
	std::string_view getClass() const final override;
};

} // namespace openmsx

#endif
