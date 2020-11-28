#ifndef JOYSTICKPORT_HH
#define JOYSTICKPORT_HH

#include "Connector.hh"
#include "openmsx.hh"

namespace openmsx {

class JoystickDevice;
class PluggingController;

class JoystickPortIf
{
public:
	virtual ~JoystickPortIf() = default;
	[[nodiscard]] virtual byte read(EmuTime::param time) = 0;
	virtual void write(byte value, EmuTime::param time) = 0;
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
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& device, EmuTime::param time) override;

	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeDirect(byte value, EmuTime::param time);

private:
	byte lastValue;
	const std::string description;
};

class DummyJoystickPort final : public JoystickPortIf
{
public:
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;
};

} // namespace openmsx

#endif
