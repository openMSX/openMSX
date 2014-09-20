#ifndef MSXMOONSOUND_HH
#define MSXMOONSOUND_HH

#include "MSXDevice.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class YMF262;
class YMF278;

class MSXMoonSound final : public MSXDevice
{
public:
	explicit MSXMoonSound(const DeviceConfig& config);
	~MSXMoonSound();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool getNew2() const;
	byte readYMF278Status(EmuTime::param time) const;

	const std::unique_ptr<YMF262> ymf262;
	const std::unique_ptr<YMF278> ymf278;

	/** Time at which instrument loading is finished. */
	EmuTime ymf278LoadTime;
	/** Time until which the YMF278 is busy. */
	EmuTime ymf278BusyTime;

	int opl3latch;
	byte opl4latch;
	bool alreadyReadID;
};
SERIALIZE_CLASS_VERSION(MSXMoonSound, 3);

} // namespace openmsx

#endif
