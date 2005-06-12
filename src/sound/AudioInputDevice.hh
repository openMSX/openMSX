// $Id$

#ifndef AUDIOINPUTDEVICE_HH
#define AUDIOINPUTDEVICE_HH

#include "Pluggable.hh"

namespace openmsx {

class AudioInputDevice : public Pluggable
{
public:
	/**
	 * Read wave data
	 */
	virtual short readSample(const EmuTime& time) = 0;

	// Pluggable
	virtual const std::string& getClass() const;
};

} // namespace openmsx

#endif
