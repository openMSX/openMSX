#ifndef MSXPSG_HH
#define MSXPSG_HH

#include "MSXDevice.hh"
#include "AY8910Periphery.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class AY8910;
class CassettePortInterface;
class RenShaTurbo;
class JoystickPortIf;

class MSXPSG final : public MSXDevice, public AY8910Periphery
{
public:
	explicit MSXPSG(const DeviceConfig& config);
	~MSXPSG();

	void reset(EmuTime::param time) override;
	void powerDown(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AY8910Periphery: port A input, port B output
	byte readA(EmuTime::param time) override;
	void writeB(byte value, EmuTime::param time) override;

	std::unique_ptr<AY8910> ay8910;
	JoystickPortIf* ports[2];
	CassettePortInterface& cassette;
	RenShaTurbo& renShaTurbo;

	int registerLatch;
	int selectedPort;
	byte prev;
	const bool keyLayoutBit;
};
SERIALIZE_CLASS_VERSION(MSXPSG, 2);

} // namespace openmsx

#endif
