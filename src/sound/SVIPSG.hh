#ifndef SVIPSG_HH
#define SVIPSG_HH

#include "AY8910.hh"
#include "AY8910Periphery.hh"

#include "MSXDevice.hh"

#include <array>

namespace openmsx {

class JoystickPortIf;

class SVIPSG final : public MSXDevice, public AY8910Periphery
{
public:
	explicit SVIPSG(const DeviceConfig& config);
	~SVIPSG() override;

	void reset(EmuTime time) override;
	void powerDown(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AY8910Periphery: port A input, port B output
	[[nodiscard]] byte readA(EmuTime time) override;
	void writeB(byte value, EmuTime time) override;

private:
	std::array<JoystickPortIf*, 2> ports;
	AY8910 ay8910; // must come after ports
	int registerLatch;
	byte prev = 255;
};

} // namespace openmsx

#endif
