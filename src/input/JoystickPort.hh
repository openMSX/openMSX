#ifndef JOYSTICKPORT_HH
#define JOYSTICKPORT_HH

#include "Connector.hh"
#include "static_string_view.hh"
#include <cstdint>

namespace openmsx {

class JoystickDevice;
class PluggingController;

class JoystickPortIf
{
public:
	virtual ~JoystickPortIf() = default;
	[[nodiscard]] virtual uint8_t read(EmuTime::param time) = 0;
	virtual void write(uint8_t value, EmuTime::param time) = 0;
protected:
	JoystickPortIf() = default;
};

class JoystickPort final : public JoystickPortIf, public Connector
{
public:
	JoystickPort(PluggingController& pluggingController,
	             std::string name, static_string_view description);

	[[nodiscard]] JoystickDevice& getPluggedJoyDev() const;

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& device, EmuTime::param time) override;

	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeDirect(uint8_t value, EmuTime::param time);

private:
	uint8_t lastValue = 255; // != 0
	const static_string_view description;
};

class DummyJoystickPort final : public JoystickPortIf
{
public:
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;
};

} // namespace openmsx

#endif
