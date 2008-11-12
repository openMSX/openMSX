// $Id$

#ifndef AUDIOINPUTCONNECTOR_HH
#define AUDIOINPUTCONNECTOR_HH

#include "Connector.hh"

namespace openmsx {

class AudioInputDevice;

class AudioInputConnector : public Connector
{
public:
	AudioInputConnector(PluggingController& pluggingController,
	                    const std::string& name);
	virtual ~AudioInputConnector();

	AudioInputDevice& getPluggedAudioDev() const;

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;

	short readSample(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
