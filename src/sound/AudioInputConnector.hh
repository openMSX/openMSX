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
	[[nodiscard]] zstring_view getDescription() const override;
	[[nodiscard]] zstring_view getClass() const override;

	[[nodiscard]] int16_t readSample(EmuTime time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
