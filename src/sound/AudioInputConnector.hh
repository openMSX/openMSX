#ifndef AUDIOINPUTCONNECTOR_HH
#define AUDIOINPUTCONNECTOR_HH

#include "Connector.hh"
#include <cstdint>

namespace openmsx {

class AudioInputDevice;

class AudioInputConnector final : public Connector
{
public:
	AudioInputConnector(PluggingController& pluggingController,
	                    std::string name);

	[[nodiscard]] AudioInputDevice& getPluggedAudioDev() const;

	// Connector
	[[nodiscard]] std::string_view getDescription() const final;
	[[nodiscard]] std::string_view getClass() const final;

	[[nodiscard]] int16_t readSample(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
