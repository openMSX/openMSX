// $Id$

#ifndef AUDIOINPUTCONNECTOR_HH
#define AUDIOINPUTCONNECTOR_HH

#include "Connector.hh"

namespace openmsx {

class PluggingController;

class AudioInputConnector : public Connector
{
public:
	AudioInputConnector(PluggingController& pluggingController,
	                    const std::string& name);
	virtual ~AudioInputConnector();

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;

	short readSample(const EmuTime& time) const;

private:
	PluggingController& pluggingController;
};

} // namespace openmsx

#endif
