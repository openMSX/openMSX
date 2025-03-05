#ifndef Y8950KEYBOARDCONNECTOR_HH
#define Y8950KEYBOARDCONNECTOR_HH

#include "Connector.hh"

#include <cstdint>

namespace openmsx {

class Y8950KeyboardDevice;

class Y8950KeyboardConnector final : public Connector
{
public:
	explicit Y8950KeyboardConnector(PluggingController& pluggingController);

	void write(uint8_t data, EmuTime::param time);
	[[nodiscard]] uint8_t read(EmuTime::param time) const;
	[[nodiscard]] uint8_t peek(EmuTime::param time) const;
	[[nodiscard]] Y8950KeyboardDevice& getPluggedKeyb() const;

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t data = 255;
};

} // namespace openmsx

#endif
