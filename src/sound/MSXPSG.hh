#ifndef MSXPSG_HH
#define MSXPSG_HH

#include "MSXDevice.hh"
#include "AY8910.hh"
#include "AY8910Periphery.hh"
#include "serialize_meta.hh"
#include <array>

namespace openmsx {

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

	std::array<JoystickPortIf*, 2> ports;
	int selectedPort;
	int registerLatch;
	byte prev;
	const byte keyLayout; // 0x40 or 0x00
	const byte addressMask; // controls address mirroring
	AY8910 ay8910; // must come after initialisation of most stuff above
};
SERIALIZE_CLASS_VERSION(MSXPSG, 2);

} // namespace openmsx

#endif
