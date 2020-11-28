#ifndef SVIPSG_HH
#define SVIPSG_HH

#include "MSXDevice.hh"
#include "AY8910Periphery.hh"
#include <memory>

namespace openmsx {

class AY8910;
class JoystickPortIf;

class SVIPSG final : public MSXDevice, public AY8910Periphery
{
public:
	SVIPSG(const DeviceConfig& config);
	~SVIPSG() override;

	void reset(EmuTime::param time) override;
	void powerDown(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AY8910Periphery: port A input, port B output
	[[nodiscard]] byte readA(EmuTime::param time) override;
	void writeB(byte value, EmuTime::param time) override;

private:
	std::unique_ptr<AY8910> ay8910;
	JoystickPortIf* ports[2];

	int registerLatch;
	byte prev;
};

} // namespace openmsx

#endif
