// $Id$

#ifndef __AUDIOINPUTCONNECTOR_HH__
#define __AUDIOINPUTCONNECTOR_HH__

#include "Connector.hh"
#include "AudioInputDevice.hh"

namespace openmsx {

class AudioInputConnector : public Connector
{
public:
	AudioInputConnector(const std::string& name);
	virtual ~AudioInputConnector();

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual AudioInputDevice& getPlugged() const;

	short readSample(const EmuTime& time) const;
};

} // namespace openmsx

#endif // __AUDIOINPUTCONNECTOR_HH__
