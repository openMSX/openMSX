#ifndef MSXHBI55_HH
#define MSXHBI55_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "SRAM.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXHBI55 final : public MSXDevice, public I8255Interface
{
public:
	explicit MSXHBI55(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	[[nodiscard]] byte readA(EmuTime::param time) override;
	[[nodiscard]] byte readB(EmuTime::param time) override;
	[[nodiscard]] nibble readC0(EmuTime::param time) override;
	[[nodiscard]] nibble readC1(EmuTime::param time) override;
	[[nodiscard]] byte peekA(EmuTime::param time) const override;
	[[nodiscard]] byte peekB(EmuTime::param time) const override;
	[[nodiscard]] nibble peekC0(EmuTime::param time) const override;
	[[nodiscard]] nibble peekC1(EmuTime::param time) const override;
	void writeA(byte value, EmuTime::param time) override;
	void writeB(byte value, EmuTime::param time) override;
	void writeC0(nibble value, EmuTime::param time) override;
	void writeC1(nibble value, EmuTime::param time) override;

	void writeStuff();
	byte readStuff() const;

private:
	I8255 i8255;
	SRAM sram;
	byte lastC; // hack to approach a 'floating value'
};
SERIALIZE_CLASS_VERSION(MSXHBI55, 2);

} // namespace openmsx

#endif
