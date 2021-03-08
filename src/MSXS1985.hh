/*
 * This class implements the
 *   backup RAM
 *   bitmap function
 * of the S1985 MSX-engine
 *
 *  TODO explanation
 */

#ifndef S1985_HH
#define S1985_HH

#include "MSXDevice.hh"
#include "MSXMemoryMapperBase.hh"
#include "MSXSwitchedDevice.hh"
#include <memory>
#include <optional>

namespace openmsx {

class SRAM;

class DummyMemoryMapper final : public MSXMemoryMapperBase
{
public:
	explicit DummyMemoryMapper(const DeviceConfig& config);
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

class MSXS1985 final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXS1985(const DeviceConfig& config);
	~MSXS1985() override;

	// MSXDevice
	void reset(EmuTime::param time) override;

	// MSXSwitchedDevice
	[[nodiscard]] byte readSwitchedIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::unique_ptr<SRAM> sram;
	std::optional<DummyMemoryMapper> dummyMapper;
	nibble address;
	byte color1;
	byte color2;
	byte pattern;
};
SERIALIZE_CLASS_VERSION(MSXS1985, 3);

} // namespace openmsx

#endif
