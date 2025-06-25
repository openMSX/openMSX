#ifndef MSXMUSIC_HH
#define MSXMUSIC_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "YM2413.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXMusicBase : public MSXDevice
{
public:
	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	explicit MSXMusicBase(DeviceConfig& config);
	~MSXMusicBase() override = default;

	void writePort(bool port, byte value, EmuTime time);

protected:
	Rom rom;
	YM2413 ym2413;
};
SERIALIZE_CLASS_VERSION(MSXMusicBase, 3);


class MSXMusic final : public MSXMusicBase
{
public:
	explicit MSXMusic(DeviceConfig& config);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};
SERIALIZE_CLASS_VERSION(MSXMusic, 3); // must be same as MSXMusicBase


// Variant used in Panasonic_FS-A1WX and Panasonic_FS-A1WSX
class MSXMusicWX final : public MSXMusicBase
{
public:
	explicit MSXMusicWX(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte control;
};
SERIALIZE_CLASS_VERSION(MSXMusicWX, 3); // must be same as MSXMusicBase

} // namespace openmsx

#endif
