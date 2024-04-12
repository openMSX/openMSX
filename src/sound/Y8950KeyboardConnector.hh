#ifndef Y8950KEYBOARDCONNECTOR_HH
#define Y8950KEYBOARDCONNECTOR_HH

#include "Connector.hh"
#include "openmsx.hh"

namespace openmsx {

class Y8950KeyboardDevice;

class Y8950KeyboardConnector final : public Connector
{
public:
	explicit Y8950KeyboardConnector(PluggingController& pluggingController);

	void write(byte data, EmuTime::param time);
	[[nodiscard]] byte read(EmuTime::param time) const;
	[[nodiscard]] byte peek(EmuTime::param time) const;
	[[nodiscard]] Y8950KeyboardDevice& getPluggedKeyb() const;

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte data = 255;
};

} // namespace openmsx

#endif
