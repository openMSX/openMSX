// $Id$

#ifndef AUDIOINPUTCONNECTOR_HH
#define AUDIOINPUTCONNECTOR_HH

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

#endif
