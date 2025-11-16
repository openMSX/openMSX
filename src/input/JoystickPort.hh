#ifndef JOYSTICKPORT_HH
#define JOYSTICKPORT_HH

#include "Connector.hh"
#include <cstdint>
#include <string>

namespace openmsx {

class JoystickDevice;
class PluggingController;

class JoystickPortIf
{
public:
	virtual ~JoystickPortIf() = default;
	[[nodiscard]] virtual uint8_t read(EmuTime time) = 0;
	virtual void write(uint8_t value, EmuTime time) = 0;
protected:
	JoystickPortIf() = default;
};

class JoystickPort final : public JoystickPortIf, public Connector
{
public:
	JoystickPort(PluggingController& pluggingController,
	             std::string name, std::string description);

	[[nodiscard]] JoystickDevice& getPluggedJoyDev() const;

	// Connector
	[[nodiscard]] zstring_view getDescription() const override;
	[[nodiscard]] zstring_view getClass() const override;
	void plug(Pluggable& device, EmuTime time) override;

	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeDirect(uint8_t value, EmuTime time);

private:
	uint8_t lastValue = 255; // != 0
	std::string description;
};

class DummyJoystickPort final : public JoystickPortIf
{
public:
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;
};

} // namespace openmsx

#endif
