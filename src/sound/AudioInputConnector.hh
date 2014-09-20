#ifndef AUDIOINPUTCONNECTOR_HH
#define AUDIOINPUTCONNECTOR_HH

#include "Connector.hh"

namespace openmsx {

class AudioInputDevice;

class AudioInputConnector final : public Connector
{
public:
	AudioInputConnector(PluggingController& pluggingController,
	                    string_ref name);

	AudioInputDevice& getPluggedAudioDev() const;

	// Connector
	const std::string getDescription() const final override;
	string_ref getClass() const final override;

	short readSample(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
