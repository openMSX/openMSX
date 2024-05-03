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
	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	explicit MSXMusicBase(const DeviceConfig& config);
	~MSXMusicBase() override = default;

	void writePort(bool port, byte value, EmuTime::param time);

protected:
	Rom rom;
	YM2413 ym2413;
};
SERIALIZE_CLASS_VERSION(MSXMusicBase, 3);


class MSXMusic final : public MSXMusicBase
{
public:
	explicit MSXMusic(const DeviceConfig& config);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};
SERIALIZE_CLASS_VERSION(MSXMusic, 3); // must be same as MSXMusicBase


// Variant used in Panasonic_FS-A1WX and Panasonic_FS-A1WSX
class MSXMusicWX final : public MSXMusicBase
{
public:
	explicit MSXMusicWX(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte control;
};
SERIALIZE_CLASS_VERSION(MSXMusicWX, 3); // must be same as MSXMusicBase

} // namespace openmsx

#endif
