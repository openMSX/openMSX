// $Id$

#ifndef __AUDIOINPUTDEVICE_HH__
#define __AUDIOINPUTDEVICE_HH__

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
