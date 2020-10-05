#ifndef MSXMOONSOUND_HH
#define MSXMOONSOUND_HH

#include "MSXDevice.hh"
#include "YMF262.hh"
#include "YMF278.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXMoonSound final : public MSXDevice
{
public:
	explicit MSXMoonSound(const DeviceConfig& config);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool getNew2() const;
	byte readYMF278Status(EmuTime::param time) const;

	YMF262 ymf262;
	YMF278 ymf278;

	/** Time at which instrument loading is finished. */
	EmuTime ymf278LoadTime;
	/** Time until which the YMF278 is busy. */
	EmuTime ymf278BusyTime;

	int opl3latch;
	byte opl4latch;
};
SERIALIZE_CLASS_VERSION(MSXMoonSound, 3);

} // namespace openmsx

#endif
