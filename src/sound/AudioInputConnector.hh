// $Id$

#ifndef __AUDIOINPUTCONNECTOR_HH__
#define __AUDIOINPUTCONNECTOR_HH__

#include "Connector.hh"
#include "AudioInputDevice.hh"

namespace openmsx {

class AudioInputConnector : public Connector
{
public:
	AudioInputConnector(const string& name);
	virtual ~AudioInputConnector();

	// Connector
	virtual const string& getDescription() const;
	virtual const string& getClass() const;
	virtual AudioInputDevice& getPlugged() const;

	short readSample(const EmuTime& time) const;
};

} // namespace openmsx

#endif // __AUDIOINPUTCONNECTOR_HH__
