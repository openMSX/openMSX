#ifndef MSXPSG_HH
#define MSXPSG_HH

#include "MSXDevice.hh"
#include "AY8910.hh"
#include "AY8910Periphery.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

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

	CassettePortInterface& cassette;
	RenShaTurbo& renShaTurbo;

	JoystickPortIf* ports[2];
	int selectedPort;
	int registerLatch;
	byte prev;
	const bool keyLayoutBit;
	// TODO could be by-value, but visual studio doesn't support
	// initialization of arrays (ports[2]) in the initializer list yet.
	std::unique_ptr<AY8910> ay8910; // must come after initialisation of most stuff above
};
SERIALIZE_CLASS_VERSION(MSXPSG, 2);

} // namespace openmsx

#endif
