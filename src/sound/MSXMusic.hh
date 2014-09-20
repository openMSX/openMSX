#ifndef MSXMUSIC_HH
#define MSXMUSIC_HH

#include "MSXDevice.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class Rom;
class YM2413;

class MSXMusic : public MSXDevice
{
public:
	explicit MSXMusic(const DeviceConfig& config);
	virtual ~MSXMusic();

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void writeRegisterPort(byte value, EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);

	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<YM2413> ym2413;

private:
	int registerLatch;
};
SERIALIZE_CLASS_VERSION(MSXMusic, 2);

} // namespace openmsx

#endif
