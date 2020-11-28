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
	~MSXPSG() override;

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
	CassettePortInterface& cassette;
	RenShaTurbo& renShaTurbo;

	JoystickPortIf* ports[2];
	int selectedPort;
	int registerLatch;
	byte prev;
	const byte keyLayout; // 0x40 or 0x00
	const byte addressMask; // controls address mirroring
	// TODO could be by-value, but visual studio doesn't support
	// initialization of arrays (ports[2]) in the initializer list yet.
	std::unique_ptr<AY8910> ay8910; // must come after initialisation of most stuff above
};
SERIALIZE_CLASS_VERSION(MSXPSG, 2);

} // namespace openmsx

#endif
