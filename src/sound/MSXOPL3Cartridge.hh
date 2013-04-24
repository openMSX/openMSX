#ifndef MSXOPL3CARTRIDGE_HH
#define MSXOPL3CARTRIDGE_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class YMF262;

class MSXOPL3Cartridge : public MSXDevice
{
public:
	explicit MSXOPL3Cartridge(const DeviceConfig& config);
	virtual ~MSXOPL3Cartridge();

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<YMF262> ymf262;
	int opl3latch;
};

} // namespace openmsx

#endif
